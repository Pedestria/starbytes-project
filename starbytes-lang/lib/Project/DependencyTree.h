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
        std::string name;
        std::string source_dir;
        std::string dump_loc;
    };

    TYPED_ENUM ModuleType;
    /*Dependency Lib or Executable for a Dependent Target*/
    struct TargetDependency {
        std::string name;
        ModuleType type;
        std::vector<std::string> dependents;
        std::string dump_loc;
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
            void add(TargetDependency & dep);
            TargetDependency & getDependencyByName(std::string & name);
            template<typename Lambda>
            void foreach(Lambda func){
                for(auto & dep : dependencies){
                    func(dep);
                }
            };
            /*Only removes if dependency does NOT have dependents AND if dependency exists!*/
            bool remove(std::string name);
    };

    void buildDependencyTreeForProject(std::vector<StarbytesModule *> &modules_to_compile);

NAMESPACE_END

#endif