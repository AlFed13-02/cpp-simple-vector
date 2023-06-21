#pragma once

#include <cassert>
#include <initializer_list>
#include <string>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <utility>

#include "array_ptr.h"

using namespace std::literals;

class ReserveProxyObj {
public:
    explicit ReserveProxyObj(size_t capacity)
        : capacity_to_reserve_(capacity){
    }
    
    size_t GetCapacityToReserve() const noexcept {
        return capacity_to_reserve_;
    }
private:
    size_t capacity_to_reserve_;        
};

template <typename Type>
class SimpleVector {
public:
    using Iterator = Type*;
    using ConstIterator = const Type*;

    SimpleVector() noexcept = default;

    // Создаёт вектор из size элементов, инициализированных значением по умолчанию
    explicit SimpleVector(size_t size)         
        : items_(ArrayPtr<Type>(size))
        , size_(size)
        , capacity_(size) {
        for (auto it = begin(); it != end(); ++it) {
            *it = Type();
        }
    }

    // Создаёт вектор из size элементов, инициализированных значением value
    SimpleVector(size_t size, Type value)
        : items_(ArrayPtr<Type>(size))
        , size_(size)
        , capacity_(size) {
        std::fill(begin(), end(), value);
    }
 
    // Создаёт вектор из std::initializer_list
    SimpleVector(std::initializer_list<Type> init) {
        Assign(init, init.size());
    }
    
    SimpleVector(const ReserveProxyObj& obj) {
        Reserve(obj.GetCapacityToReserve());
    }
    
    SimpleVector(const SimpleVector& other) {
        Assign(other, other.size_);    
    }
    
    SimpleVector& operator=(const SimpleVector& rhs) {
        if (!(this == &rhs)) {
            SimpleVector tmp(rhs);
            swap(tmp);
        }
        return *this;
    }
    
    SimpleVector(SimpleVector&& other) {
        items_ = std::move(other.items_);
        size_ = std::exchange(other.size_, 0);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    
    SimpleVector& operator=(SimpleVector&&) = default;
    
    void Reserve(size_t new_capacity){
        if (capacity_ < new_capacity) {
            Reallocate(new_capacity);
        }
    }
    
     // Обменивает значение с другим вектором
    void swap(SimpleVector& other) noexcept {
         items_.swap(other.items_);
         std::swap(size_, other.size_);
         std::swap(capacity_, other.capacity_);
    }
    
     // Добавляет элемент в конец вектора
    // При нехватке места увеличивает вдвое вместимость вектора
    void PushBack(Type item) {
        if (capacity_ == 0) {
            Reallocate(1);
        }
        if (size_ == capacity_) {
            Reallocate(capacity_ * 2);
        }
        
        items_[size_] = std::move(item);
        ++size_;
    }
    
    // Вставляет значение value в позицию pos.
    // Возвращает итератор на вставленное значение
    // Если перед вставкой значения вектор был заполнен полностью,
    // вместимость вектора должна увеличиться вдвое, а для вектора вместимостью 0 стать равной 1
    Iterator Insert(ConstIterator pos, Type value) {
        assert(pos >= cbegin() && pos <= cend());
        if (size_ < capacity_) {
            auto it = const_cast<Iterator>(pos);
            std::move_backward(it, end(), end() + 1);
            *it = std::move(value);
            ++size_;
            return it;
        } else {
            size_t new_capacity = (size_ == 0 ? 1 : capacity_ * 2); 
            SimpleVector tmp(new_capacity);
            auto it = std::move(begin(), const_cast<Iterator>(pos), tmp.begin());
            *it = std::move(value);
            std::move(const_cast<Iterator>(pos), end(), it + 1);
            tmp.size_ = size_ + 1;
            swap(tmp);
            return it;
        }
    }
    
    // "Удаляет" последний элемент вектора. Вектор не должен быть пустым
    void PopBack() noexcept {
        assert(!IsEmpty());
        --size_;
    }
    
    // Удаляет элемент вектора в указанной позиции
    Iterator Erase(ConstIterator pos) {
        assert(pos >= cbegin() && pos < cend());
        auto it = const_cast<Iterator>(pos);
        std::move(it + 1, end(), it);
        --size_;
        return it;
    }
    
    
    
    // Возвращает количество элементов в массиве
    size_t GetSize() const noexcept {
        return size_;
    }

    // Возвращает вместимость массива
    size_t GetCapacity() const noexcept {
        return capacity_;
    }

    // Сообщает, пустой ли массив
    bool IsEmpty() const noexcept {
        return size_ == 0;
    }

    // Возвращает ссылку на элемент с индексом index
    Type& operator[](size_t index) noexcept {
        assert(index < size_);
        return items_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    const Type& operator[](size_t index) const noexcept {
        assert(index < size);
        return items_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    Type& At(size_t index) {
        if (index >= size_) {
            throw std::out_of_range("index out of range"s);
        }
        return items_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    const Type& At(size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("index out of range"s);
        }
        return items_[index];
    }

    // Обнуляет размер массива, не изменяя его вместимость
    void Clear() noexcept {
        size_ = 0;
    }

    // Изменяет размер массива.
    // При увеличении размера новые элементы получают значение по умолчанию для типа Type
    void Resize(size_t new_size) {
        if (new_size > capacity_) {
            auto new_capacity = std::max(new_size, capacity_ * 2);
            Reallocate(new_capacity);
        }
        
        if (new_size > size_) {
            for (auto it = end(); it != end() + new_size - size_; ++it){
                *it = Type();
            }
        }
        
        size_ = new_size;
    }

    // Возвращает итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator begin() noexcept {
        return items_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator end() noexcept {
        return begin() + size_;
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator begin() const noexcept {
        return items_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator end() const noexcept {
        return begin() + size_;
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cbegin() const noexcept {
        return begin();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cend() const noexcept {
        return end();
    }
private:
    ArrayPtr<Type> items_;
    size_t size_ = 0;
    size_t capacity_ = 0;
    
    template <typename Values>
    void Assign(const Values& values, size_t size) {
        SimpleVector tmp(size);
        std::move(values.begin(), values.end(), tmp.begin());
        swap(tmp);
    }
    
    void Reallocate(size_t new_capacity) { 
        ArrayPtr<Type> new_array(new_capacity); 
        std::move(items_.Get(), items_.Get() + size_, new_array.Get());
        items_.swap(new_array);
        capacity_ = new_capacity;
    }
};

template <typename Type>
inline bool operator==(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    if (&lhs == &rhs) {
        return true;
    }
    if (lhs.GetSize() == rhs.GetSize()) {
	    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
    }
    return false;
}

template <typename Type>
inline bool operator!=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs == rhs);
}

template <typename Type>
inline bool operator<(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename Type>
inline bool operator<=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(rhs < lhs);
}

template <typename Type>
inline bool operator>(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return rhs < lhs;
}

template <typename Type>
inline bool operator>=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return rhs <= lhs;
} 

ReserveProxyObj Reserve(size_t capacity_to_reserve) {
    return ReserveProxyObj(capacity_to_reserve);
}