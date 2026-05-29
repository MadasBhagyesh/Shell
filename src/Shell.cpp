//
// Created by kostia on 11.12.25.
//

#include "Shell.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <unistd.h>
#include <readline/readline.h>
#include <sys/wait.h>
#include <cstdio>
#include <readline/history.h>

using std::string, std::vector, std::optional;
namespace fs = std::filesystem;


std::ostream &operator<<(std::ostream &stream, const ParsedLine &parsed_line) {
    stream << "command: " << parsed_line.command << '\n';
    stream << "[";
    for (const auto &arg: parsed_line.args) {
        stream << arg << ' ';
    }
    stream << "]";
    stream << "[";
    for (const auto &[specifier, target]: parsed_line.redirects) {
        stream << specifier << ':' << target << ' ';
    }
    stream << "]\n";
    return stream;
}

optional<fs::path> Shell::check_path(const string &command_name) {
    string path_env(getenv("PATH"));
    std::stringstream path_ss{path_env};
    string directory_path;

    while (std::getline(path_ss, directory_path, ':')) {
        fs::path command_path = fs::path(directory_path).append(command_name);

        if (!std::filesystem::exists(command_path)) {
            continue;
        }

        if (access(command_path.c_str(), X_OK) == 0) {
            return {command_path};
        }
    }

    return {};
}

vector<string> Shell::get_path_executables() {
    string path_env(getenv("PATH"));
    std::stringstream path_ss{path_env};
    string directory_path;

    vector<string> path_executables;

    while (std::getline(path_ss, directory_path, ':')) {
        fs::path path_dir = fs::path(directory_path);

        for (const auto &entry: fs::directory_iterator(path_dir)) {
            if (access(entry.path().c_str(), X_OK) == 0) {
                path_executables.push_back(entry.path().filename());
            }
        }
    }

    return path_executables;
}

Shell::Shell() : shell_input(std::cin), shell_output(std::cout), shell_error(std::cerr) {
}

std::map<std::string, std::function<void(Shell &, std::vector<std::string> &)> > Shell::builtins{
    {"echo", &Shell::echo},
    {"exit", &Shell::exit},
    {"pwd", &Shell::pwd},
    {"type", &Shell::type},
    {"cd", &Shell::cd},
};

ParsedLine Shell::get_args(const string &line) {
    ParsedLine parsed{};

    std::stringstream input{line};
    std::stringstream last_arg{};

    std::stringstream redirect_specifier{};
    bool is_redirect = false;

    size_t length{0};

    char c;
    input >> std::noskipws;

    while (input >> c) {
        if (isspace(c)) {
            if (length != 0) {
                if (is_redirect) {
                    parsed.redirects[redirect_specifier.str()] = last_arg.str();
                    is_redirect = false;
                } else {
                    parsed.args.push_back(last_arg.str());
                }
                last_arg.str("");
                length = 0;
            }
        } else if (c == '\'') {
            while (input >> c) {
                if (c == '\'') {
                    break;
                }

                last_arg << c;
                ++length;
            }
        } else if (c == '"') {
            while (input >> c) {
                if (c == '"') {
                    break;
                }

                if (c == '\\') {
                    input >> c;

                    switch (c) {
                        case '\\':
                        case '$':
                        case '`':
                        case '\n':
                        case '"': {
                            last_arg << c;
                            break;
                        }
                        default: {
                            last_arg << '\\' << c;
                            break;
                        }
                    }
                } else {
                    last_arg << c;
                    ++length;
                }
            }
        } else if (c == '\\') {
            input >> c;
            last_arg << c;
            ++length;
        } else if (c == '>' || c == '<') {
            if (!is_redirect) {
                redirect_specifier = std::move(last_arg);
                redirect_specifier << c;
                last_arg.str("");
                length = 0;

                is_redirect = true;
            } else {
                redirect_specifier << c;
            }
        } else {
            last_arg << c;
            ++length;
        }
    }

    if (length != 0) {
        if (is_redirect) {
            parsed.redirects[redirect_specifier.str()] = last_arg.str();
        } else {
            parsed.args.push_back(last_arg.str());
        }
    }

    if (parsed.args.size() != 0) {
        parsed.command = parsed.args[0];
        parsed.args.erase(parsed.args.cbegin());
    }

    input >> std::skipws;

    return parsed;
}

std::vector<std::string> Shell::get_pipeline(const std::string &line) {
    std::stringstream input{line};
    string pipeline_stage;
    vector<string> pipeline;
    while (std::getline(input, pipeline_stage, '|')) {
        pipeline.push_back(pipeline_stage);
    }

    return pipeline;
}

char *Shell::get_completions(const char *text, int state) {
    // This function is called with state=0 the first time; subsequent calls are
    // with a nonzero state. state=0 can be used to perform one-time
    // initialization for this completion session.
    static std::vector<std::string> matches;
    static size_t match_index = 0;

    if (state == 0) {
        // During initialization, compute the actual matches for 'text' and keep
        // them in a static vector.
        matches.clear();
        match_index = 0;

        // Collect a vector of matches: vocabulary words that begin with text.
        std::string text_str{text};
        for (const auto &builtin: builtins | std::views::keys) {
            if (builtin.size() >= text_str.size() &&
                builtin.compare(0, text_str.size(), text_str) == 0) {
                matches.push_back(builtin);
            }
        }

        for (const auto &path_executable: get_path_executables()) {
            if (path_executable.size() >= text_str.size() &&
                path_executable.compare(0, text_str.size(), text_str) == 0) {
                matches.push_back(path_executable);
            }
        }

        std::sort(matches.begin(), matches.end());
    }

    if (match_index >= matches.size()) {
        // We return nullptr to notify the caller no more matches are available.
        return nullptr;
    } else {
        // Return a malloc'd char* for the match. The caller frees it.
        return strdup(matches[match_index++].c_str());
    }
}

char **completer(const char *text, int start, int end) {
    rl_attempted_completion_over = 1;

    return rl_completion_matches(text, Shell::get_completions);
}

void Shell::run() {
    rl_attempted_completion_function = completer;

    for (;;) {
        char* c_line = readline("$ ");

        if (!c_line) {
            break;
        }

        string line{c_line};
        auto pipeline{get_pipeline(line)};

        int pipe_descriptors_prev[2];

        for (const auto &[i, pipeline_stage]: std::views::enumerate(pipeline)) {
            auto parsed_stage = get_args(pipeline_stage);

            int pipe_descriptors_next[2];
            if (i != pipeline.size() - 1) {
                pipe(pipe_descriptors_next);
            }

            std::vector<const char *> args_c;
            args_c.reserve(parsed_stage.args.size() + 2);

            args_c.push_back(parsed_stage.command.c_str());

            for (const auto &arg: parsed_stage.args) {
                args_c.push_back(arg.c_str());
            }
            args_c.push_back(nullptr);

            if (!fork()) {
                if (i != 0) {
                    dup2(pipe_descriptors_prev[0], STDIN_FILENO);
                    close(pipe_descriptors_prev[0]);
                    close(pipe_descriptors_prev[1]);
                } else {
                    if (parsed_stage.redirects.contains("<")) {
                        std::FILE *input = std::fopen(parsed_stage.redirects["<"].c_str(), "r");
                        dup2(input->_fileno, STDIN_FILENO);
                        std::fclose(input);
                    }
                }

                if (i != pipeline.size() - 1) {
                    dup2(pipe_descriptors_next[1], STDOUT_FILENO);
                    close(pipe_descriptors_next[0]);
                    close(pipe_descriptors_next[1]);
                } else {
                    std::FILE *output = nullptr;

                    if (parsed_stage.redirects.contains(">")) {
                        output = std::fopen(parsed_stage.redirects[">"].c_str(), "w");
                    }
                    if (parsed_stage.redirects.contains(">>")) {
                        output = std::fopen(parsed_stage.redirects[">>"].c_str(), "a");
                    }

                    if (output) {
                        dup2(output->_fileno, STDOUT_FILENO);
                        std::fclose(output);
                    }
                }


                execvp(args_c[0], const_cast<char *const *>(args_c.data()));

                perror("execvp");
                std::exit(1);
            }

            close(pipe_descriptors_prev[0]);
            close(pipe_descriptors_prev[1]);
            std::swap(pipe_descriptors_prev, pipe_descriptors_next);
        }

        while (wait(nullptr) > 0) {
        }

        // open_redirects(args.redirects);
        //
        // if (builtins.contains(args.command)) {
        //     builtins.at(args.command)(*this, args.args);
        // } else if (auto path = check_path(args.command)) {
        //     run_system(args.command, args.args);
        // } else {
        //     shell_output.get() << args.command << ": command not found" << '\n';
        // }
        //
        // close_redirects();
        //
        // if (requested_exit) {
        //     break;
        // }
    }
}

void Shell::open_redirects(std::map<std::string, std::string> &redirects) {
    for (const auto &[specifier, target]: redirects) {
        if (specifier == ">" || specifier == "1>") {
            file_output.open(target);
            shell_output = std::ref(file_output);
        } else if (specifier == ">>" || specifier == "1>>") {
            file_output.open(target, std::ios_base::app);
            shell_output = std::ref(file_output);
        } else if (specifier == "2>") {
            file_error.open(target);
            shell_error = std::ref(file_error);
        } else if (specifier == "2>>") {
            file_error.open(target, std::ios_base::app);
            shell_error = std::ref(file_error);
        } else if (specifier == "<") {
            file_input.open(target);
            shell_input = std::ref(file_input);
        }
    }
}

void Shell::close_redirects() {
    if (file_output.is_open()) {
        file_output.close();
        shell_output = std::cout;
    }

    if (file_error.is_open()) {
        file_error.close();
        shell_error = std::cerr;
    }

    if (file_input.is_open()) {
        file_input.close();
        shell_input = std::cin;
    }
}
