#ifndef SHELL_STARTER_CPP_SHELL_H
#define SHELL_STARTER_CPP_SHELL_H
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>
#include <map>

struct ParsedLine {
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> redirects;
};

std::ostream &operator<<(std::ostream &stream, const ParsedLine &parsed_line);

class Shell {
    bool requested_exit{false};
    std::ifstream file_input;
    std::ofstream file_output;
    std::ofstream file_error;

    std::reference_wrapper<std::istream> shell_input;
    std::reference_wrapper<std::ostream> shell_output;
    std::reference_wrapper<std::ostream> shell_error;

    static ParsedLine get_args(const std::string &line);

    static std::vector<std::string> get_pipeline(const std::string &line);

    static std::optional<std::filesystem::path> check_path(const std::string &command_name);
public:
    static std::vector<std::string> get_path_executables();

    void echo(std::vector<std::string> &args);

    void exit(std::vector<std::string> &args);

    void pwd(std::vector<std::string> &args);

    void cd(std::vector<std::string> &args);

    void type(std::vector<std::string> &args);

    void open_redirects(std::map<std::string, std::string> &redirects);

    void close_redirects();

public:
    static char *get_completions(const char *text, int state);

    static std::map<std::string, std::function<void(Shell &, std::vector<std::string> &)> > builtins;

    Shell();

    void run();
};


#endif //SHELL_STARTER_CPP_SHELL_H
