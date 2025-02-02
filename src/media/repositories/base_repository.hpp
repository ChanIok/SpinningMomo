#pragma once

#include <vector>

template<typename T, typename ID>
class IRepository {
public:
    virtual T find_by_id(ID id) = 0;
    virtual std::vector<T> find_all(bool include_deleted = false) = 0;
    virtual bool save(T& entity) = 0;
    virtual bool remove(ID id) = 0;
    virtual ~IRepository() = default;
}; 