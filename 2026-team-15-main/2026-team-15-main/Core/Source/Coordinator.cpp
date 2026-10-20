#include "Coordinator.hpp"

namespace Ecs{

    void Coordinator::Init()
    {
        // Create pointers to each manager
        mComponentManager = std::make_unique<ComponentManager>();
        mEntityManager = std::make_unique<EntityManager>();
        mSystemManager = std::make_unique<SystemManager>();
    }
   

    Entity Coordinator::CreateEntity()
    {
        return mEntityManager->CreateEntity();
    }

    Entity Coordinator::CreateEntityWithId(Entity id){
        return mEntityManager->CreateEntityWithId(id);
    }

    void Coordinator::DestroyEntity(Entity entity)
    {
        mEntityManager->DestroyEntity(entity);

        mComponentManager->EntityDestroyed(entity);

        mSystemManager->EntityDestroyed(entity);
    }

    bool Coordinator::isAlive(Entity et) const{
        return mEntityManager->isLiving(et);
    }

    std::vector<Entity> Coordinator::LivingEntities() const{
        return mEntityManager->LivingEntities();
    }
    
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