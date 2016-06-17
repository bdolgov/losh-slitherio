#include "alloc.hpp"

using namespace mem;

chunk::chunk(size_t size):
	memory(static_cast<char*>(malloc(size)), chunk_deleter()),
	pos(memory.get()),
	end(pos + size),
	next(nullptr)
{
}

void chunk_deleter::operator()(void *p) const
{
	free(static_cast<void*>(p));
}

arena::arena(size_t _advise, size_t _chunk_size):
	chunk_size(_chunk_size ? _chunk_size : _advise / 8),
	total(0),
	first(new chunk(_advise + chunk_size)),
	current(first.get())
{
}

size_t arena::get_total() const
{
	return total;
}

void* arena::alloc_mem(size_t size, size_t align)
{
	intptr_t orig_pos = reinterpret_cast<intptr_t>(current->pos), aligned_pos = orig_pos;

	if (aligned_pos & (align - 1))
	{
		aligned_pos = (aligned_pos | (align - 1)) + 1;
	}

	intptr_t moved_pos = aligned_pos + size;

	if (moved_pos > reinterpret_cast<intptr_t>(current->end))
	{
		grow(size);
		return alloc_mem(size, align);
	}

	current->pos = reinterpret_cast<char*>(moved_pos);
	total += moved_pos - orig_pos;
	return reinterpret_cast<void*>(aligned_pos);
}

void arena::grow(size_t size)
{
	current->next.reset(new chunk(chunk_size >= size ? chunk_size : size));
	current = current->next.get();
}
