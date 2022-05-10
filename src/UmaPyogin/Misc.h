#ifndef UMAPYOGIN_MISC_H
#define UMAPYOGIN_MISC_H

#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace UmaPyogin
{
	using OpaqueFunctionPointer = void (*)();
	using HookFunction = void (*)(OpaqueFunctionPointer addr, OpaqueFunctionPointer hook,
	                              OpaqueFunctionPointer* orig);
	using SymbolLookupFunction = OpaqueFunctionPointer (*)(void* library, const char* name);

	namespace Misc
	{
		std::u16string ToUTF16(const std::string_view& str);
		std::string ToUTF8(const std::u16string_view& str);

		namespace Parallel
		{
			template <typename ForwardIterator, typename Sentinel, typename Callable>
			void ForEach(ForwardIterator begin, Sentinel end, Callable&& func,
			             std::size_t concurrentSize = std::thread::hardware_concurrency())
			{
				std::vector<std::thread> threads(concurrentSize);
				std::size_t totalSize = 0;
				auto it = begin;
				// std::distance 无法处理 Sentinel，因此手动计算
				while (it != end)
				{
					++totalSize;
					++it;
				}
				const auto chunkSize = totalSize / concurrentSize;
				it = begin;
				for (auto i = 0; i < concurrentSize; ++i)
				{
					it = std::next(it, chunkSize);
					threads[i] = std::thread([&, it]() mutable {
						for (auto j = 0; j < chunkSize; ++j)
						{
							std::forward<Callable>(func)(*it);
							++it;
						}
					});
				}
				for (auto& t : threads)
				{
					t.join();
				}
			}

			template <typename ForwardIterator, typename Sentinel, typename Callable>
			void OnePassForEach(ForwardIterator begin, Sentinel end, Callable&& func,
			                    std::size_t concurrentSize = std::thread::hardware_concurrency())
			{
				std::vector<std::thread> threads(concurrentSize);
				std::vector values(begin, end);
				const auto totalSize = values.size();
				const auto chunkSize = totalSize / concurrentSize;
				for (auto i = 0; i < concurrentSize; ++i)
				{
					threads[i] = std::thread([&, base = i * chunkSize]() {
						for (auto j = 0; j < chunkSize; ++j)
						{
							std::forward<Callable>(func)(std::move(values[base + j]));
						}
					});
				}
				for (auto& t : threads)
				{
					t.join();
				}
			}
		} // namespace Parallel
	}     // namespace Misc
} // namespace UmaPyogin

#endif
