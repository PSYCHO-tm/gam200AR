#include "Coordinator.hpp"

namespace Ecs{

    /*
        Initializes and creates Unique ptrs for the ECS managers
        MUST BE CALLED after every Coordinator calls
    */
    void Coordinator::Init()
    {
        // Create pointers to each manager
        mComponentManager = std::make_unique<ComponentManager>();
        mEntityManager = std::make_unique<EntityManager>();
        mSystemManager = std::make_unique<SystemManager>();
    }
   

    /*
        Generates a new Entity 
    */
    Entity Coordinator::CreateEntity()
    {
        assert(NumLiving() < MAX_ENTITIES && "Critical Error: World capacity limit exceeded!");
        return mEntityManager->CreateEntity();
    }

    /*
        Generates a new entity via a pre-determined id
    */
    Entity Coordinator::CreateEntityWithId(Entity id){
        assert(NumLiving() < MAX_ENTITIES && "Critical Error: World capacity limit exceeded!");
        return mEntityManager->CreateEntityWithId(id);
    }

    /*
        Locates an entity within the map and destroy it
    */
    void Coordinator::DestroyEntity(Entity entity)
    {
        mEntityManager->DestroyEntity(entity);

        mComponentManager->EntityDestroyed(entity);

        mSystemManager->EntityDestroyed(entity);
    }

    /*
       Checks whether a given entity ID is currently active and living in the world.
    */
    bool Coordinator::isAlive(Entity et) const{
        return mEntityManager->isLiving(et);
    }

    /*
        Returns a vector list of all currently active entities 
    */
    std::vector<Entity> Coordinator::LivingEntities() const{
        return mEntityManager->LivingEntities();
    }
    
    /*
        Iterates through all active entities and safely destroys them all at once.
    */
    void Coordinator::DestroyAll() noexcept{
        for (Entity e : mEntityManager->LivingEntities())
        {
            DestroyEntity(e);      
        }
    }

    uint32_t Coordinator::NumLiving() const { 
        return mEntityManager->NumLivingCount(); 
    }
}