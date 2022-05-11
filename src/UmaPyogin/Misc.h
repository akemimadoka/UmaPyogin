#ifndef UMAPYOGIN_MISC_H
#define UMAPYOGIN_MISC_H

#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

#if __has_include(<concepts>)
#include <concepts>
#define HAS_CONCEPTS 1
#endif

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
#if HAS_CONCEPTS
			template <std::forward_iterator ForwardIterator, typename Sentinel, typename Callable>
			void
#else
			template <typename ForwardIterator, typename Sentinel, typename Callable>
			std::enable_if_t<std::is_base_of_v<
			    std::forward_iterator_tag,
			    typename std::iterator_traits<ForwardIterator>::iterator_category>>
#endif
			ForEach(ForwardIterator begin, Sentinel end, Callable&& func,
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

#if HAS_CONCEPTS
			template <std::input_iterator InputIterator, typename Sentinel, typename Callable>
			void
#else
			template <typename InputIterator, typename Sentinel, typename Callable>
			std::enable_if_t<
			    std::is_base_of_v<std::input_iterator_tag,
			                      typename std::iterator_traits<InputIterator>::iterator_category>>
#endif
			OnePassForEach(InputIterator begin, Sentinel end, Callable&& func,
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
