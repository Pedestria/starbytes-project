#include "DependencyTree.h"

STARBYTES_STD_NAMESPACE

void DependencyTree::add(TargetDependency & dep) {
    dependencies.push_back(dep);
};

bool DependencyTree::remove(std::string name) {
    bool return_code = false;
    for(int i = 0;i < dependencies.size();++i){
        TargetDependency & d = dependencies[i];
        if(d.name == name && d.dependents.empty()){
            dependencies.erase(dependencies.begin()+i);
            return_code = true;
            break;
        } 
    }
    return return_code;
};

Foundation::Unsafe<TargetDependency> DependencyTree::getDependencyByName(std::string &name){
    for(auto & dep : dependencies){
        if(dep.name == name){
            return dep;
        }
    }
    return Foundation::Error("Dependency Not Found!");
}

NAMESPACE_END