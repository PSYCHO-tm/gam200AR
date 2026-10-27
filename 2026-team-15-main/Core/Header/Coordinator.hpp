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
            Initializes and creates Unique ptrs for the ECS managers
            MUST BE CALLED after every Coordinator calls
        */
        void Init();


        /*
            Make a new Entity
        */
        Entity CreateEntity();

        /*
		    Generates a new entity via a pre-determined id
	    */
        Entity CreateEntityWithId(Entity id);

        /*
		    Iterates through all active entities and safely destroys them all at once
	    */
        void DestroyEntity(Entity entity);

       
        /*
		    Checks whether a given entity ID is currently active and living in the world
	    */
        bool isAlive(Entity et) const;
        
        /*
		    Returns a vector list of all currently active entities
	    */
        std::vector<Entity> LivingEntities() const;


        // Component methods
		/*
		    Registers a new component type with the component manager
	    */
        template<typename T>
        void RegisterComponent();

        /*
		    Adds a component to an entity and updates its signature and system subscriptions
	    */
        template<typename T>
        void AddComponent(Entity entity, T component);

        /*
		    Checks if an entity possesses a specific component type
	    */
        template<typename T>
        bool HasComponent(Entity e) const;

        /*
		    Removes a component from an entity and updates its signature and system subscriptions
	    */
        template<typename T>
        void RemoveComponent(Entity entity);

        /*
             Retrieves a reference to an entity's component
        */
        template<typename T>
        T& GetComponent(Entity entity);

        /*
		    Retrieves a constant reference to an entity's component
	    */
        template<typename T>
        T const& GetComponent(Entity entity) const;

        /*
		    Returns the unique numerical type identifier for a component
	    */
        template<typename T>
        ComponentType GetComponentType();


        // System methods
		/*
		    Registers a new system type with the system manager
	    */
        template<typename T>
        std::shared_ptr<T> RegisterSystem();

        /*
		    Sets the component signature mask required for a system to process entities
	    */
        template<typename T>
        void SetSystemSignature(Signature signature);

        //Additional
		/*
		    Destroys every living entity in the world sequentiallys
	    */
        void DestroyAll() noexcept;

        /*
		    Returns the total count of currently active living entities.
	    */
        uint32_t NumLiving() const;

    private:
        std::unique_ptr<ComponentManager> mComponentManager;
        std::unique_ptr<EntityManager> mEntityManager;
        std::unique_ptr<SystemManager> mSystemManager;
    };
   
}


//Component Side coordinator 
namespace Ecs{

	/*
	    Registers a new component type with the component manager
	*/
    template<typename T>
    void Coordinator::RegisterComponent()
    {
        mComponentManager->RegisterComponent<T>();
    }

     /*
	    Adds a component to an entity and updates its signature and system subscriptions
	*/
    template<typename T>
    void Coordinator::AddComponent(Entity entity, T component)
    {
        mComponentManager->AddComponent<T>(entity, component);

        auto signature = mEntityManager->GetSignature(entity);
        signature.set(mComponentManager->GetComponentType<T>(), true);
        mEntityManager->SetSignature(entity, signature);

        mSystemManager->EntitySignatureChanged(entity, signature);
    }

    /*
	    Checks if an entity possesses a specific component type
	*/
    template<typename T>
    bool Coordinator::HasComponent(Entity e) const{
        return mComponentManager->HasComponent<T>(e);
    }

    /*
	    Removes a component from an entity and updates its signature and system subscriptions
	*/
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
	/*
	      Retrieves a reference to an entity's component
	*/
    template<typename T>
    T& Coordinator::GetComponent(Entity entity)
    {
        return mComponentManager->GetComponent<T>(entity);
    }

    /*
	      Retrieves a const reference to an entity's component
	*/
    template<typename T>
    T const& Coordinator::GetComponent(Entity entity) const
    {
        return mComponentManager->GetComponent<T>(entity);
    }

    /*
	    Returns the unique numerical type identifier for a component
	*/
    template<typename T>
    ComponentType Coordinator::GetComponentType()
    {
        return mComponentManager->GetComponentType<T>();
    }


    // System methods
	/*
	    Registers a new system type with the system manager
	*/
    template<typename T>
    std::shared_ptr<T> Coordinator::RegisterSystem()
    {
        return mSystemManager->RegisterSystem<T>();
    }

    /*
	    Sets the component signature mask required for a system to process entities
	*/
    template<typename T>
    void Coordinator::SetSystemSignature(Signature signature)
    {
        mSystemManager->SetSignature<T>(signature);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

