#pragma once

#include "vcs/repository.hpp"

namespace vcs {

void command_add(const Repository& repo, int argc, char** argv);
void command_branch(const Repository& repo, int argc, char** argv);
void command_checkout(const Repository& repo, int argc, char** argv);
void command_commit(const Repository& repo, int argc, char** argv);
void command_init(int argc, char** argv);
void command_log(const Repository& repo);
void command_remove(const Repository& repo, int argc, char** argv);
void command_restore(const Repository& repo, int argc, char** argv);
void command_revert(const Repository& repo, int argc, char** argv);
void command_status(const Repository& repo);

class VcsApp {
public:
    int run(int argc, char** argv) const;
};

} // namespace vcs
