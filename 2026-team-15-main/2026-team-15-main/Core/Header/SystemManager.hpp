#pragma once
#include "sharedlib.hpp"

namespace Ecs{

	class System{
		public:
		std::vector<Entity> entityVec{};

	};

	/*
	Systems contain code but no state / data. Each system declares a bitmask signature to be compared
	--> Used in Physics / Renderer
	*/

    class SystemManager{
	public:
		
		/*
			Checks if type hasn't been recorded and records it within mSystems
		*/
		template<typename T>
		std::shared_ptr<T> RegisterSystem();

		/*
			Sets system signature inside mSignature
		*/
		template<typename T>
		void SetSignature(Signature signature);

		/*
			Checks for a specific, destroyed entity and erase it from all system lists
		*/
		void EntityDestroyed(Entity entity);

		/*
			Compares an entities signature by bitwise AND with another signature.
       		Then checks if entity exists within vector, before adding / removing it
		*/
		void EntitySignatureChanged(Entity entity, Signature entitySignature);


		//Helper Functions

		/*
			Returns pointer to what T is stored in mSystems
		*/
		template<typename T>
		std::shared_ptr<T> GetSystem() const;

		/*
			Returns total number of entities currently processed by entityVec
		*/
		template<typename T>
		size_t GetEntityCount() const;

		/*
        	Clears the entity data within each system
    	*/
		void Reset();
		

	private:
		// Map from system type string to a signature
		std::unordered_map<std::string, Signature> mSignatures{};

		// Map from system type string to a system pointer
		std::unordered_map<std::string, std::shared_ptr<System>> mSystems{};
	};
}


namespace Ecs{


		template<typename T>
		std::shared_ptr<T> SystemManager::RegisterSystem()
		{
			std::string typeName = typeid(T).name();

			assert(mSystems.find(typeName) == mSystems.end() && "[RegisterSystem] Registering system more than once.");

			// Create a pointer to the system
			auto system = std::make_shared<T>();
			mSystems.insert({typeName, system});
			
			return system;
		}


		template <typename T>
		void SystemManager::SetSignature(Signature signature)
		{
			std::string typeName = typeid(T).name();

			assert(mSystems.find(typeName) != mSystems.end() && " [SetSignature] System used before registered.");

			// Set the signature for this system
			mSignatures[typeName] = signature;
		}


		template<typename T>
		std::shared_ptr<T> SystemManager::GetSystem() const {
			std::string typeName = typeid(T).name();
			assert(mSystems.find(typeName) != mSystems.end() && "[GetSystem] System used before registered.");

			return std::static_pointer_cast<T>(mSystems.at(typeName));
		}


		template<typename T>
		size_t SystemManager::GetEntityCount() const {
			std::string typeName = typeid(T).name();
			assert(mSystems.find(typeName) != mSystems.end() && "[GetEntityCount] System not registered.");

			return mSystems.at(typeName)->entityVec.size();
		}
}