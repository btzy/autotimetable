#include <cstdint>

#include <utility>
#include <tuple>
#include <vector>
#include <algorithm>
#include <limits>
#include <iterator>
#include <type_traits>

#include "autotimetable.hpp"
#include "intrinsics.hpp"

#ifndef AUTOTIMETABLE_FILTER_DEPTH
#define AUTOTIMETABLE_FILTER_DEPTH 3
#endif

namespace autotimetable {

	// moves el towards begin until sequence becomes sorted
	template <typename RandomAccessIterator, typename Compare>
	inline RandomAccessIterator sort_single_element_towards_front(RandomAccessIterator begin, RandomAccessIterator el, Compare comp) {
		while (el > begin && comp(*el, *(el - 1))) {
			swap(*el, *(el - 1));
			--el;
		}
		return el;
	}

	// moves el towards end until target
	template <typename RandomAccessIterator>
	inline void unsort_single_element_from_front(RandomAccessIterator el, RandomAccessIterator target) {
		while (el < target) {
			swap(*el, *(el + 1));
			++el;
		}
	}


	inline score_t calculate_score(const timeblock& timeblock, const score_config& scorer) noexcept {
		score_t answer = 0;
		std::for_each(timeblock.days, timeblock.days + TIMEBLOCK_DAY_COUNT, [&answer, &scorer](const timeblock_day_t& day) {
			if (day != 0) {
				answer += scorer.travel_penalty;
				answer += (intrinsics::find_largest_set(day) - intrinsics::find_smallest_set(day) + 1) * scorer.empty_slot_penalty;
				if (((~day) & scorer.lunch_time) == 0) {
					answer += scorer.no_lunch_penalty;
				}
			}
		});
		return answer;
	}

	// assumes that there are no clashing lessons
	inline void add_timeblock(score_t& score, timeblock& dest, const timeblock& src, const score_config& scorer) noexcept {
		// this function is slower than optimal as it recalculates the score
		// could probably be made faster

		dest.add(src);
		score = calculate_score(dest, scorer);
	}

	// assumes that there are no clashing lessons
	inline void remove_timeblock(score_t& score, timeblock& dest, const timeblock& src, const score_config& scorer) noexcept {
		// this function is slower than optimal as it recalculates the score
		// could probably be made faster

		dest.remove(src);
		score = calculate_score(dest, scorer);
	}



	struct search_item {
		typename std::vector<mod>::const_iterator mod_it;
		typename std::vector<mod_item>::const_iterator mod_item_it;
		std::vector<std::pair<timeblock, typename std::vector<mod_item_choice>::const_iterator>> choices;
	};

	inline void swap(search_item& a, search_item& b) {
		swap(a.mod_it, b.mod_it);
		swap(a.mod_item_it, b.mod_item_it);
		swap(a.choices, b.choices);
	}


	typedef typename std::vector<search_item>::iterator search_iterator_t;


	score_t _find_best_impl(const search_iterator_t next, const search_iterator_t end, score_t current_score, timetable& current_timetable, score_t best_score, timetable& best_timetable, const score_config& scorer);

	inline void _do_find_best_iteration(const search_iterator_t& next, const search_iterator_t& end, score_t& current_score, timetable& current_timetable, score_t& best_score, timetable& best_timetable, const score_config& scorer) {
		search_iterator_t pass_next = next;
		++pass_next;

		for (auto it3 = next->choices.cbegin(); it3 != next->choices.cend(); ++it3) {

			if (current_timetable.timeblock.clash(it3->first))continue;

			// add the current choice to the current timetable
			add_timeblock(current_score, current_timetable.timeblock, it3->first, scorer);
			current_timetable.items.emplace_back(next->mod_it, next->mod_item_it, it3->second);

			// recursive call
			best_score = _find_best_impl(pass_next, end, current_score, current_timetable, best_score, best_timetable, scorer);

			// remove the current choice
			current_timetable.items.pop_back();
			remove_timeblock(current_score, current_timetable.timeblock, it3->first, scorer);

		}
	}

	
	// lower score is better
	// best_timetable is the output, this function will only overwrite it if the score is better than current_best
	// current_timetable may be modified in this function, but all modifications must be reversed upon returning from this function
	// return value should be at most best_score (return value == best_score means that nothing better can be found)
	score_t _find_best_impl(const search_iterator_t next, const search_iterator_t end, score_t current_score, timetable& current_timetable, score_t best_score, timetable& best_timetable, const score_config& scorer) {
		if (next == end) {
			if (current_score < best_score) { // we've found something better than ever!
				// keep this better result instead of the old result
				best_timetable = current_timetable;
				best_score = current_score;
			}
			return best_score;
		}

		// if we reach here, it means next < end, i.e. we have some more mod_items to place on the timetable
		// we will then place the next item on the timetable and recursively call this function again

		if (std::distance(next, end) > (AUTOTIMETABLE_FILTER_DEPTH)) {
			search_iterator_t filter_it = next;
			std::advance(filter_it, AUTOTIMETABLE_FILTER_DEPTH);

			std::vector<std::pair<timeblock, typename std::vector<mod_item_choice>::const_iterator>> tmp_choices = std::move(filter_it->choices);

			filter_it->choices.clear();
			filter_it->choices.reserve(tmp_choices.size());

			std::copy_if(tmp_choices.cbegin(), tmp_choices.cend(), std::back_inserter(filter_it->choices), [&current_timetable](const std::pair<timeblock, typename std::vector<mod_item_choice>::const_iterator>& choice) {
				return !current_timetable.timeblock.clash(choice.first);
			});

			search_iterator_t next_new = next;
			++next_new;
			search_iterator_t new_filter_it = sort_single_element_towards_front(next_new, filter_it, [](const search_item& a, const search_item& b) {
				return a.choices.size() < b.choices.size();
			});

			_do_find_best_iteration(next, end, current_score, current_timetable, best_score, best_timetable, scorer);

			unsort_single_element_from_front(new_filter_it, filter_it);

			filter_it->choices = std::move(tmp_choices);

		}
		else {
			_do_find_best_iteration(next, end, current_score, current_timetable, best_score, best_timetable, scorer);
		}

		// return the new best_score
		return best_score;
	}

	timetable find_best(std::vector<search_item>&& mod_its, const score_config& scorer) {

		// the answer will go here
		timetable best_timetable;

		// the current (temp) timetable being built
		timetable current_timetable;

		// we shall process the module-item with least choices first (it might be faster this way)
		std::sort(mod_its.begin(), mod_its.end(), [](const search_item& a, const search_item& b) {
			return a.choices.size() < b.choices.size();
		});

		// lets go!
		_find_best_impl(mod_its.begin(), mod_its.end(), 0, current_timetable, std::numeric_limits<score_t>::max(), best_timetable, scorer);

		return best_timetable;
	}

	timetable find_best(const std::vector<mod>& mods, const score_config& scorer) {

		std::vector<search_item> mod_its;

		for (auto it1 = mods.cbegin(); it1 != mods.cend(); ++it1) {
			for (auto it2 = it1->items.cbegin(); it2 != it1->items.cend(); ++it2) {
				std::vector<std::pair<timeblock, typename std::vector<mod_item_choice>::const_iterator>> tmp_vec;
				tmp_vec.reserve(it2->choices.size());
				for (auto it3 = it2->choices.cbegin(); it3 != it2->choices.cend(); ++it3) {
					// the if-statement is to prevent mods with many options from making the engine slow by only taking the first of similar options
					// it turns out that this optimization yields more than 5x increase in speed
					if (std::find_if(tmp_vec.cbegin(), tmp_vec.cend(), [&tb = it3->timeblock](const std::pair<timeblock, typename std::vector<mod_item_choice>::const_iterator>& choice) {
						return choice.first == tb;
					}) == tmp_vec.cend())tmp_vec.emplace_back(it3->timeblock, it3);
				}
				tmp_vec.shrink_to_fit();
				mod_its.emplace_back(search_item{ it1, it2, std::move(tmp_vec) });
			}
		}

		// be nice to the system, don't keep memory we will never use
		mod_its.shrink_to_fit();

		return find_best(std::move(mod_its), scorer);

	}

}