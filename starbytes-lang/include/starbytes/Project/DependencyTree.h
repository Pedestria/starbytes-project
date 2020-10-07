#include <string>
#include <vector>
#include "starbytes/Base/Base.h"
#include "starbytes/Base/Macros.h"

#ifndef PROJECT_DEPENDENCY_H
#define PROJECT_DEPENDENCY_H

STARBYTES_STD_NAMESPACE

    class StarbytesModule{
    public:
        StarbytesModule();

    };
    /*Dependency Lib or Executable for a Dependent Target*/
    struct TargetDependency {
        std::string name;
        std::vector<std::string> dependents;
    };

    class DependencyTree {
        private:
            std::vector<TargetDependency> dependencies;
        public:
            DependencyTree(std::string entry):entry_point_target(entry){};
            ~DependencyTree(){
                dependencies.clear();
            };
            std::string entry_point_target;
            void add(TargetDependency && dep);
            /*Only removes if dependency does NOT have dependents AND if dependency exists!*/
            bool remove(std::string name);
    };

    void buildDependencyTreeForProject(std::vector<StarbytesModule *> &modules_to_compile);

NAMESPACE_END

#endif