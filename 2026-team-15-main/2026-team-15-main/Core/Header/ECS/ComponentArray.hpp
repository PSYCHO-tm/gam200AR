#pragma once
#include "sharedlib.hpp"

namespace Ecs{

class IComponentArray
{
public:
	virtual ~IComponentArray() = default;
	virtual void EntityDestroyed(Entity entity) = 0;
};


template <typename T>
class ComponentArray : public IComponentArray
{
public:
	/*
		Initializes dense data array with new componentData 
	*/
	void InsertData(Entity entity, T componentData);

	/*
        Removes entity by doing a manual "pop_back"
	*/
	void RemoveData(Entity entity);

    /*
		Retrieves data from entity given
	*/
	T& GetData(Entity entity);

    /*
		Retrieves data from entity given (const version)
	*/
	T const& GetData(Entity entity) const;

    /*
		Returns total size of data
	*/
	size_t GetSize() const noexcept;

    /*
		Additional function that checks entity position
	*/
	bool HasData(Entity entity) const;

    /*
		Checks if entity still exists, if such destroy it.
	*/
	void EntityDestroyed(Entity entity) override;

private:

	//Packed Contiguous Array where data lives
	std::array<T, MAX_ENTITIES> mComponentArray; 

	//Both unordered_maps are considered "DOUBLE MAPPING" to preserve O(1) lookup
	// Map Entity ID --> Dense Array index
	std::unordered_map<Entity, size_t> entityToIndexMap;
  

	// Map Array index --> Entity ID
	std::unordered_map<size_t, Entity> indexToEntityMap;

	size_t totalSize{};
};

}

//================================================TEMPLATE CLASS DEFINITIONS====================================================
namespace Ecs{

	/*
		Initializes dense data array with new componentData 
	*/
	template <typename T>
	void ComponentArray<T>::InsertData(Entity entity, T componentData)
	{
		assert(entityToIndexMap.find(entity) == entityToIndexMap.end() && " [InsertData] Component added to same entity more than once.");

		//Creating new entry and updating both maps
		size_t newIndex = totalSize;
		entityToIndexMap[entity] = newIndex;

		indexToEntityMap[newIndex] = entity;

		//Initialize data array with component data
		mComponentArray[newIndex] = std::move(componentData);

		++totalSize;
	}

	/*
	 	This is all done to ensure that array does not shift down N numbers of time after deletion

	*/
	template <typename T>
	void ComponentArray<T>::RemoveData(Entity entity)
	{
		auto const posToRemove = entityToIndexMap.find(entity);
		assert(posToRemove!= entityToIndexMap.end() && " [RemoveData] Removing non-existent component.");

		//Getting index of current and last entity
		size_t removedIndex = posToRemove->second;
		size_t indexOfLast = totalSize - 1;

		if(removedIndex != indexOfLast)
		{
			Entity lastEntity = indexToEntityMap[indexOfLast];

			//Swapping memory so that is essentially a 'pop_back'
			std::swap(mComponentArray[removedIndex], mComponentArray[indexOfLast]);

			// Update map to point to moved spot
			entityToIndexMap[lastEntity] = removedIndex;
			indexToEntityMap[removedIndex] = lastEntity;
		}

		indexToEntityMap.erase(indexOfLast); 
		entityToIndexMap.erase(posToRemove); 
		--totalSize;
	}

	template <typename T>
	T& ComponentArray<T>::GetData(Entity entity) {
		auto const pos = entityToIndexMap.find(entity);
		assert(pos != entityToIndexMap.end() && " [GetData] Retrieving non-existent component.");

		return mComponentArray[pos->second];
	}

	template <typename T>
	T const& ComponentArray<T>::GetData(Entity entity) const {
		auto const pos = entityToIndexMap.find(entity);
		assert(pos != entityToIndexMap.end() && "[GetData] Retrieving non-existent component.");

		return mComponentArray[pos->second];
	}

	template <typename T>
	size_t ComponentArray<T>::GetSize() const noexcept{
		return totalSize;
	}

	template <typename T>
	bool ComponentArray<T>::HasData(Entity entity) const{
		return entityToIndexMap.find(entity) != entityToIndexMap.end();
	}

	template <typename T>
	void ComponentArray<T>::EntityDestroyed(Entity entity)
	{
		if (HasData(entity))
		{
			RemoveData(entity);
		}
	}

}
