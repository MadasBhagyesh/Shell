#include "Shell.h"
#include <filesystem>

using std::string, std::vector, std::optional, std::filesystem::path;


void Shell::echo(vector<string> &args) {
    for (const auto &arg: args) {
        shell_output.get() << arg << ' ';
    }
    shell_output.get() << '\n';
}

void Shell::exit(vector<string> &args) {
    requested_exit = true;
}

void Shell::pwd(vector<string> &args) {
    shell_output.get() << std::filesystem::current_path().string() << '\n';
}

void Shell::cd(vector<string> &args) {
    const string &directory = args.at(0);

    if (std::filesystem::exists(path(directory))) {
        std::filesystem::current_path(path(directory));
    } else if (directory == "~") {
        std::filesystem::current_path(path(getenv("HOME")));
    } else {
        shell_output.get() << "cd: " << directory << ": No such file or directory\n";
    }
}

void Shell::type(vector<string> &args) {
    const string &command = args.at(0);

    if (builtins.contains(command)) {
        shell_output.get() << command << " is a shell builtin\n";
    } else if (auto command_path = Shell::check_path(command)) {
        shell_output.get() << command << " is " << command_path.value().string() << '\n';
    } else {
        shell_output.get() << command << ": not found\n";
    }
}
