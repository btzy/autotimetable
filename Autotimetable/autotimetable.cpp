#include <cstdint>

#include <utility>
#include <tuple>
#include <vector>
#include <algorithm>
#include <limits>
#include <type_traits>

#include "autotimetable.hpp"
#include "intrinsics.hpp"

namespace autotimetable {


	inline score_t calculate_score(const timeblock& timeblock, const score_config& scorer) noexcept {
		score_t answer = 0;
		std::for_each(timeblock.days, timeblock.days + TIMEBLOCK_DAY_COUNT, [&answer, &scorer](const timeblock_day_t& day) {
			if (day != 0) {
				answer += scorer.travel_penalty;
				answer += (intrinsics::find_first_set(day) - intrinsics::find_last_set(day) + 1) * scorer.empty_slot_penalty;
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






	typedef std::pair<typename std::vector<mod>::const_iterator, typename std::vector<mod_item>::const_iterator> search_item_t;
	typedef typename std::vector<search_item_t>::const_iterator search_iterator_t;


	
	// lower score is better
	// best_timetable is the output, this function will only overwrite it if the score is better than current_best
	// current_timetable may be modified in this function, but all modifications must be reversed upon returning from this function
	// return value should be at most best_score (return value == best_score means that nothing better can be found)
	score_t _find_best_impl(search_iterator_t next, search_iterator_t end, score_t current_score, timetable& current_timetable, score_t best_score, timetable& best_timetable, const score_config& scorer) {
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

		for (auto it3 = next->second->choices.cbegin(); it3 != next->second->choices.cend(); ++it3) {

			if (current_timetable.timeblock.clash(it3->timeblock))continue;

			// add the current choice to the current timetable
			add_timeblock(current_score, current_timetable.timeblock, it3->timeblock, scorer);
			current_timetable.items.emplace_back(next->first, next->second, it3);

			// recursive call
			best_score = _find_best_impl(next + 1, end, current_score, current_timetable, best_score, best_timetable, scorer);

			// remove the current choice
			current_timetable.items.pop_back();
			remove_timeblock(current_score, current_timetable.timeblock, it3->timeblock, scorer);

		}

		// return the new best_score
		return best_score;
	}

	timetable find_best(std::vector<search_item_t>&& mod_its, const score_config& scorer) {

		// the answer will go here
		timetable best_timetable;

		// the current (temp) timetable being built
		timetable current_timetable;

		// we shall process the module-item with least choices first (it might be faster this way)
		std::sort(mod_its.begin(), mod_its.end(), [](const typename std::decay_t<decltype(mod_its)>::value_type& a, const typename std::decay_t<decltype(mod_its)>::value_type& b) {
			return a.second->choices.size() < b.second->choices.size();
		});

		// lets go!
		_find_best_impl(mod_its.cbegin(), mod_its.cend(), 0, current_timetable, std::numeric_limits<score_t>::max(), best_timetable, scorer);

		return best_timetable;
	}

	timetable find_best(const std::vector<mod>& mods, const score_config& scorer) {

		std::vector<search_item_t> mod_its;

		for (auto it1 = mods.cbegin(); it1 != mods.cend(); ++it1) {
			for (auto it2 = it1->items.cbegin(); it2 != it1->items.cend(); ++it2) {
				mod_its.emplace_back(it1, it2);
			}
		}

		// be nice to the system, don't keep memory we will never use
		mod_its.shrink_to_fit();

		return find_best(std::move(mod_its), scorer);

	}

}