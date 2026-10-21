#pragma once
#include "sharedlib.hpp"
#include "SystemManager.hpp"
#include "EntityManager.hpp"
#include "ComponentManager.hpp"

namespace Ecs{
    class Coordinator
    {
    public:
        /*
            Initalize at the start to create unique_ptrs
        */
        void Init();


        // Entity methods
        Entity CreateEntity();

        void DestroyEntity(Entity entity);

        Entity CreateEntityWithId(Entity id);
        
        bool isAlive(Entity et) const;
        
        std::vector<Entity> LivingEntities() const;


        // Component methods
        template<typename T>
        void RegisterComponent();

        template<typename T>
        void AddComponent(Entity entity, T component);

        template<typename T>
        bool HasComponent(Entity e) const;

        template<typename T>
        void RemoveComponent(Entity entity);

        template<typename T>
        T& GetComponent(Entity entity);

        template<typename T>
        T const& GetComponent(Entity entity) const;

        template<typename T>
        ComponentType GetComponentType();


        // System methods
        template<typename T>
        std::shared_ptr<T> RegisterSystem();

        template<typename T>
        void SetSystemSignature(Signature signature);

        //Additional
        void DestroyAll() noexcept;
        uint32_t NumLiving() const;

    private:
        std::unique_ptr<ComponentManager> mComponentManager;
        std::unique_ptr<EntityManager> mEntityManager;
        std::unique_ptr<SystemManager> mSystemManager;
    };
   
}


//Component Side coordinator 
namespace Ecs{
    template<typename T>
    void Coordinator::RegisterComponent()
    {
        mComponentManager->RegisterComponent<T>();
    }

    template<typename T>
    void Coordinator::AddComponent(Entity entity, T component)
    {
        mComponentManager->AddComponent<T>(entity, component);

        auto signature = mEntityManager->GetSignature(entity);
        signature.set(mComponentManager->GetComponentType<T>(), true);
        mEntityManager->SetSignature(entity, signature);

        mSystemManager->EntitySignatureChanged(entity, signature);
    }

    template<typename T>
    bool Coordinator::HasComponent(Entity e) const{
        return mComponentManager->HasComponent<T>(e);
    }

    template<typename T>
    void Coordinator::RemoveComponent(Entity entity)
    {
        mComponentManager->RemoveComponent<T>(entity);

        auto signature = mEntityManager->GetSignature(entity);
        signature.set(mComponentManager->GetComponentType<T>(), false);
        mEntityManager->SetSignature(entity, signature);

        mSystemManager->EntitySignatureChanged(entity, signature);
    }
}

//System Side Coordinatir
namespace Ecs{
    
    template<typename T>
    T& Coordinator::GetComponent(Entity entity)
    {
        return mComponentManager->GetComponent<T>(entity);
    }

    template<typename T>
    T const& Coordinator::GetComponent(Entity entity) const
    {
        return mComponentManager->GetComponent<T>(entity);
    }

    template<typename T>
    ComponentType Coordinator::GetComponentType()
    {
        return mComponentManager->GetComponentType<T>();
    }


    // System methods
    template<typename T>
    std::shared_ptr<T> Coordinator::RegisterSystem()
    {
        return mSystemManager->RegisterSystem<T>();
    }

    template<typename T>
    void Coordinator::SetSystemSignature(Signature signature)
    {
        mSystemManager->SetSignature<T>(signature);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

