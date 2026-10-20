#include "sharedlib.hpp"
#include "EntityManager.hpp"


namespace Ecs{
    /*
        Default Constructor: Per construct, initialize the queue and propogate it with ordered numbers
        Same for this function, might be changed
    */
    EntityManager::EntityManager(){
        availableEntities.reserve(MAX_ENTITIES);
        Reset();
    }

    /*
        Checks if number of living object is lesser then MAX number
        Create a unique id subsequently by taking the number of the Last index [LIFO]
    */
    Entity EntityManager::CreateEntity() noexcept{
        assert(livingCount < MAX_ENTITIES && "[CreateEntity] Too many entities in existence.");

        // Take an ID from the back of the queue
        Entity id = availableEntities.back();
        availableEntities.pop_back();

        entityStatus[id] = true;

        ++livingCount;

        return id;
    }

    /*
        Checks if ID given by user already exist, or is out of scope
        Uses said unique id and removes it from the avaliable entries to prevent duplicates 
    */
    Entity EntityManager::CreateEntityWithId(Entity id) noexcept{
        assert(id < MAX_ENTITIES && "[CreateEWithId] Entity given is out of range.");
        assert(!entityStatus[id] && "[CreateEWithId] Entity is already alive." );
       
        auto pos = std::find(availableEntities.begin(), availableEntities.end(), id);
        assert(pos != availableEntities.end() && "[CreateEWithId] Id given is not available.");
        
        availableEntities.erase(pos);

        entityStatus[id] = true;
        ++livingCount;

        return id;
    }

    /*
        Resets all bits within sig[index] to be 0
    */
    void EntityManager::DestroyEntity(Entity entity) noexcept{
        assert(entity < MAX_ENTITIES && "[Destroy] Entity out of range.");
        assert(entityStatus[entity] && "[Destroy] Entity is not alive."); //to protect against double deletion
        
        signatureArr[entity].reset();
        entityStatus[entity] = false;

        availableEntities.push_back(entity);
        
        --livingCount;
    }

    /*
        Performs a bitwise copy assignment of the entire bitset passed to function
    */
    void EntityManager::SetSignature(Entity entity, Signature sig) noexcept {
        assert(entity < MAX_ENTITIES && "[SetSignature] Entity out of range.");
        assert(entityStatus[entity] && "[SetSignature] Entity is not active.");

        signatureArr[entity] = sig;
    }

        /*
        Returns the bitwise signature of the specific entity
    */
    Signature const& EntityManager::GetSignature(Entity entity) const noexcept{
        assert(entity < MAX_ENTITIES && "[GetSignature] Entity out of range.");

        return signatureArr[entity];
    }


    //Additional functions 
    /*
        Returns the number of living entities
    */
    uint32_t EntityManager::NumLivingCount() const noexcept{
        return livingCount;
    }

    /*
        Returns whether entity is alive 
    */
    bool EntityManager::isLiving(Entity entity) const{
        if (entity >= MAX_ENTITIES) return false;  
        
        return entityStatus[entity];
    }


    /*
       Returns the list of entities used required by the coordinator  
    */
    std::vector<Entity> EntityManager::LivingEntities() const{
        std::vector<Entity> list;
        list.reserve(livingCount);
            
        for (Entity e = 0; e < MAX_ENTITIES; ++e)
        {
            if (entityStatus[e]) list.push_back(e);
        }
            
        return list;
    }

    /*
       Resets the availableEntities and signatureArrays
    */
    void EntityManager::Reset() noexcept{
        
        availableEntities.clear();
        entityStatus.assign(MAX_ENTITIES, false);
        
        for(Entity e = MAX_ENTITIES; e-- > 0 ;){
            signatureArr[e].reset();
            availableEntities.push_back(e);
        }

        livingCount = 0;
    }
}