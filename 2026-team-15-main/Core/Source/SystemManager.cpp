
#include "SystemManager.hpp"

namespace Ecs{
    /*
        Checks for a specific, destroyed entity and erase it from all system lists
    */
    void SystemManager::EntityDestroyed(Entity entity){
        for (auto const& pair : mSystems)
        {
            auto& system = pair.second; //Per entity in system

            auto pos = std::find(system->entityVec.begin(), system->entityVec.end(), entity);

            if(pos != system->entityVec.end()){

                //Copy last element of vector, and pop vector
                *pos = system->entityVec.back();    
                system->entityVec.pop_back();
            }
            
        }
    }

    /*
        Compares an entities signature by bitwise AND with another signature.
        Then checks if entity exists within vector, before adding / removing it
    */
    void SystemManager::EntitySignatureChanged(Entity entity, Signature otherSignature){
        // Notify each system that an entity's signature changed
        for (auto const& pair : mSystems)
        {
            auto const& type = pair.first;
            auto const& system = pair.second;
            auto sigPos = mSignatures.find(type);

            if (sigPos == mSignatures.end()) continue;
            
            auto const& systemSignature = sigPos->second;

            auto pos = std::find(system->entityVec.begin(), system->entityVec.end(), entity);
            
            bool containsEntity = (pos != system->entityVec.end());
            bool signatureMatches = ((systemSignature & otherSignature) == systemSignature);

            if(signatureMatches){
           
                //Deny duplicated entities being recorded in vector
                if(!containsEntity) system->entityVec.push_back(entity);
            }
            else
            {
                // Entity signature does not match system signature - erase from vector
                if(containsEntity){
                    *pos = system->entityVec.back();
                    system->entityVec.pop_back();
                }
            }
        }
    }

    /*
        Clears the entity data within each system
    */
    void SystemManager::Reset() {
        for (auto const& pair : mSystems) {
            pair.second->entityVec.clear();
        }
    }
}