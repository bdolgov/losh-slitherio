#ifndef ALLOC_H

#include <memory>
#include <cassert>
#include <stdexcept>

namespace mem
{
	struct chunk_deleter
	{
		void operator()(void*) const;
	};

	struct chunk
	{
		chunk(size_t size);
		std::unique_ptr<char, chunk_deleter> memory;
		char *pos, *end;
		std::unique_ptr<chunk> next;
	};

	class arena
	{
		private:
			size_t chunk_size;
			size_t total;
			std::unique_ptr<chunk> first;
			chunk *current;
			
		public:
			arena(size_t _advise, size_t _chunk_size = 0);
			size_t get_total() const;

			void grow(size_t size);

			void *alloc_mem(size_t size, size_t align);

			template<class T, class... Args>
			T* alloc(Args&&... args)
			{
				T *ret = static_cast<T*>(alloc_mem(sizeof(T), alignof(T)));
				new(ret) T(std::forward<Args>(args)...);
				// No memory leak here: pool allocator cannot delete memory
				return ret;
			}

			template<class T, class... Args>
			T* alloc_array(size_t n, Args&&... args)
			{
				T *ret = static_cast<T*>(alloc_mem(sizeof(T) * n, alignof(T)));
				for (size_t i = 0; i < n; ++i)
				{
					new (ret + i) T(std::forward<Args>(args)...);
				}
				return ret;
			}

			template<class T>
			T* alloc_array_copy(const T* array, size_t n)
			{
				T *ret = static_cast<T*>(alloc_mem(sizeof(T) * n, alignof(T)));
				for (size_t i = 0; i < n; ++i)
				{
					new (ret + i) T(array[i]);
				}
				return ret;
			}
	};

	struct arena_deleter
	{
		void operator()(void*) const {}
	};

	template<class T>
	class dynarr
	{
		private:
			size_t m_size;
			T* m_data;
		public:
			dynarr():
				m_size(0),
				m_data(nullptr)
			{
			}

			explicit dynarr(arena& _arena, size_t _size = 0):
				m_size(_size),
				m_data(_arena.alloc_array<T>(m_size))
			{
			}

			dynarr(arena& _arena, const dynarr<T>& other):
				m_size(other.size()),
				m_data(_arena.alloc_array_copy(other.data(), other.size()))
			{
			}

			void alloc(arena& _arena, size_t size)
			{
				if (m_size)
				{
					throw std::runtime_error("Reallocating already allocated array!");
				}
				m_size = size;
				m_data = _arena.alloc_array<T>(m_size);
			}

			void realloc(arena& _arena, size_t size)
			{
				if (size <= m_size)
				{
					m_size = size;
				}
				else
				{
					throw std::runtime_error("Not implemented yet!");
				}
			}

			size_t size() const { return m_size; }
			T* data() { return m_data; }
			const T* data() const { return m_data; }
			T& operator[](size_t idx) { return m_data[idx]; }
			const T& operator[](size_t idx) const { return m_data[idx]; }
			T* begin() { return m_data; }
			T* end() { return m_data + m_size; }
			const T* begin() const { return m_data; }
			const T* end() const { return m_data + m_size; }
	};

	template<class T> T* begin(dynarr<T>& arr) { return arr.begin(); }
	template<class T> T* end(dynarr<T>& arr) { return arr.end(); }
}

#endif
