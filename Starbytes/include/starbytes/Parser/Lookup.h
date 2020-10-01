

#include <vector>
#include "starbytes/Base/Base.h"

#ifndef PARSER_LOOKUP_H
#define PARSER_LOOKUP_H 

STARBYTES_STD_NAMESPACE

	template<class Item>
	class LookupArray
	{
		private:
			std::vector<Item> internalLookup;
		public:

			LookupArray(std::initializer_list<Item> _input) : internalLookup(_input) {}
			// Lookup an item in the dictionary
			bool lookup(const Item& item) {
				for (Item it : internalLookup) {
					if (it == item) {
						return true;
					}
				}
				return false;
			}
			void push(const Item& item) {
				internalLookup.push_back(item);
			}
	};

	/*template<class Item>
	bool LookupArray<Item>::lookup(const Item& item) {
		for (Item it : internalLookup) {
			if (it == item) {
				return true;
			}
		}
		return false;
	}*/
	/*template<class Item>
	void LookupArray<Item>::push(const Item& item) {
		internalLookup.push_back(item);
	}*/

NAMESPACE_END

#endif