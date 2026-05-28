#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace dbms {

// Custom dynamic array to replace std::vector
template <typename T>
class Vector {
private:
    T* data_;
    size_t size_;
    size_t capacity_;

    void reallocate(size_t new_capacity) {
        if (new_capacity == 0) new_capacity = 1;
        T* new_data = reinterpret_cast<T*>(new char[new_capacity * sizeof(T)]);
        
        for (size_t i = 0; i < size_; ++i) {
            new (new_data + i) T(std::move(data_[i]));
            data_[i].~T();
        }
        
        delete[] reinterpret_cast<char*>(data_);
        data_ = new_data;
        capacity_ = new_capacity;
    }

public:
    Vector() : data_(nullptr), size_(0), capacity_(0) {}
    
    explicit Vector(size_t count) : data_(nullptr), size_(0), capacity_(0) {
        resize(count);
    }
    
    ~Vector() {
        clear();
        delete[] reinterpret_cast<char*>(data_);
    }
    
    Vector(const Vector& other) : data_(nullptr), size_(0), capacity_(0) {
        reserve(other.size_);
        for (size_t i = 0; i < other.size_; ++i) {
            new (data_ + i) T(other.data_[i]);
        }
        size_ = other.size_;
    }
    
    Vector(Vector&& other) noexcept : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    
    Vector& operator=(const Vector& other) {
        if (this != &other) {
            clear();
            reserve(other.size_);
            for (size_t i = 0; i < other.size_; ++i) {
                new (data_ + i) T(other.data_[i]);
            }
            size_ = other.size_;
        }
        return *this;
    }
    
    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            clear();
            delete[] reinterpret_cast<char*>(data_);
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    void push_back(const T& value) {
        if (size_ == capacity_) {
            reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        new (data_ + size_) T(value);
        ++size_;
    }

    void push_back(T&& value) {
        if (size_ == capacity_) {
            reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        new (data_ + size_) T(std::move(value));
        ++size_;
    }

    template <typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ == capacity_) {
            reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
        }
        new (data_ + size_) T(std::forward<Args>(args)...);
        ++size_;
    }

    void pop_back() {
        if (size_ > 0) {
            data_[size_ - 1].~T();
            --size_;
        }
    }

    void resize(size_t new_size) {
        if (new_size > capacity_) {
            reallocate(new_size);
        }
        if (new_size > size_) {
            for (size_t i = size_; i < new_size; ++i) {
                new (data_ + i) T();
            }
        } else {
            for (size_t i = new_size; i < size_; ++i) {
                data_[i].~T();
            }
        }
        size_ = new_size;
    }

    void reserve(size_t new_capacity) {
        if (new_capacity > capacity_) {
            reallocate(new_capacity);
        }
    }

    void clear() {
        for (size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }

    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
    
    T& at(size_t index) {
        if (index >= size_) throw std::out_of_range("Vector::at");
        return data_[index];
    }
    const T& at(size_t index) const {
        if (index >= size_) throw std::out_of_range("Vector::at");
        return data_[index];
    }

    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }
    
    void erase(size_t index) {
        if (index >= size_) return;
        data_[index].~T();
        for (size_t i = index; i < size_ - 1; ++i) {
            new (data_ + i) T(std::move(data_[i + 1]));
            data_[i + 1].~T();
        }
        --size_;
    }
};

} // namespace dbms
