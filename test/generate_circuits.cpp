#include "include/map_reducer.hpp"

void generate_data_dmmr(const size_t tile_size = 10000) {
	// simulate data
	std::vector<emp::Integer> did_1;
	std::vector<emp::Integer> did_1_b;
	std::vector<emp::Integer> did_2;
	std::vector<emp::Integer> did_2_b;
	std::vector<emp::Bit> confirmed;
	std::vector<emp::Bit> confirmed_b;
	for (size_t i = 0; i < tile_size; i++) {
		did_1.emplace_back(emp::Integer(256, tile_size - i, emp::ALICE));
		did_2.emplace_back(emp::Integer(256, i, emp::ALICE));
		confirmed.emplace_back(emp::Bit(1, emp::ALICE));
	}
	for (size_t i = 0; i < tile_size; i++) {
		did_1_b.emplace_back(emp::Integer(256, i, emp::BOB));
		did_2_b.emplace_back(emp::Integer(256, tile_size - i, emp::BOB));
		confirmed_b.emplace_back(emp::Bit(0, emp::BOB));
	}
	for (size_t i = 0; i < tile_size; i++) {
		did_1[i] = did_1[i] ^ did_1_b[i];
		did_2[i] = did_2[i] ^ did_2_b[i];
		confirmed[i] = confirmed[i] ^ confirmed_b[i];
	}
	emp::Integer sick = did_1[0];
}

void generate_data_dmr(const size_t tile_size = 10000, const size_t output_size = 500) {
	// simulate data
	std::vector<emp::Integer> did_1;
	std::vector<emp::Integer> did_1_b;
	std::vector<emp::Integer> did_2;
	std::vector<emp::Integer> did_2_b;
	std::vector<emp::Integer> lists;
	std::vector<emp::Integer> lists_b;
	std::vector<emp::Bit> confirmed;
	std::vector<emp::Bit> confirmed_b;
	for (size_t i = 0; i < tile_size; i++) {
		did_1.emplace_back(emp::Integer(256, tile_size - i, emp::ALICE));
		did_2.emplace_back(emp::Integer(256, i, emp::ALICE));
		confirmed.emplace_back(emp::Bit(1, emp::ALICE));
	}
	for (size_t i = 0; i < 2*tile_size; i++)
		lists.emplace_back(emp::Integer(32, i, emp::ALICE));
	for (size_t i = 0; i < tile_size; i++) {
		did_1_b.emplace_back(emp::Integer(256, i, emp::BOB));
		did_2_b.emplace_back(emp::Integer(256, tile_size - i, emp::BOB));
		confirmed_b.emplace_back(emp::Bit(0, emp::BOB));
	}
	for (size_t i = 0; i < 2*tile_size; i++)
		lists_b.emplace_back(emp::Integer(32, 0, emp::BOB));
	for (size_t i = 0; i < tile_size; i++) {
		did_1[i] = did_1[i] ^ did_1_b[i];
		did_2[i] = did_2[i] ^ did_2_b[i];
		confirmed[i] = confirmed[i] ^ confirmed_b[i];
	}
	for (size_t i = 0; i < 2*tile_size; i++)
		lists[i] = lists[i] ^ lists_b[i];
	emp::Integer sick = did_1[0];
}

void test_dmmr(const size_t tile_size = 10000) {
	// simulate data
	std::vector<emp::Integer> did_1;
	std::vector<emp::Integer> did_1_b;
	std::vector<emp::Integer> did_2;
	std::vector<emp::Integer> did_2_b;
	std::vector<emp::Bit> confirmed;
	std::vector<emp::Bit> confirmed_b;
	for (size_t i = 0; i < tile_size; i++) {
		did_1.emplace_back(emp::Integer(256, tile_size - i, emp::ALICE));
		did_2.emplace_back(emp::Integer(256, i, emp::ALICE));
		confirmed.emplace_back(emp::Bit(1, emp::ALICE));
	}
	for (size_t i = 0; i < tile_size; i++) {
		did_1_b.emplace_back(emp::Integer(256, i, emp::BOB));
		did_2_b.emplace_back(emp::Integer(256, tile_size - i, emp::BOB));
		confirmed_b.emplace_back(emp::Bit(0, emp::BOB));
	}
	for (size_t i = 0; i < tile_size; i++) {
		did_1[i] = did_1[i] ^ did_1_b[i];
		did_2[i] = did_2[i] ^ did_2_b[i];
		confirmed[i] = confirmed[i] ^ confirmed_b[i];
	}
	emp::Integer sick = did_1[0];

	// start mapping
	const emp::Integer none(32, -2147483648, emp::PUBLIC);
	const emp::Bit one_bit(1, emp::PUBLIC);

	// first map
	std::vector<emp::Integer> tile;
	for (size_t i = 0; i < tile_size; i++) {
		tile.emplace_back(
				emp::If((did_1[i] == sick) & (confirmed[i] == one_bit),
					(did_2[i]).resize(32), none));
	}

	// second map
	for (size_t i = 0; i < tile_size; i++) {
		tile.emplace_back(
				emp::If((did_1[i] == sick) & (confirmed[i] == one_bit),
					(did_2[i]).resize(32), none));
	}

	// sort the tiles
	emp::sort(&tile[0], tile.size());
	// for (size_t j = 0; j < tile.size(); j++) {
	//  std::cout << "Merged Element (total " << tile.size() << ") " << j <<
	//	    ": " << tile[j].reveal<int>() << std::endl;
	//}
	// mark duplicates, pre-process for compaction
	std::vector<emp::Integer> distance =
		compute_distance_mark_duplicates(&tile[0], 2 * tile_size);
	// compact the list pushing duplicates at the end
	compact(distance, &tile[0], 2 * tile_size);
	//for (size_t j = 0; j < tile.size(); j++) {
	//   std::cout << "Compact Element (total " << tile_size << ") " << j << ": " 
	//		    << tile[j].reveal<int>() << std::endl;
	//	}
}

void test_dmr(const size_t tile_size = 10000, const size_t output_size = 500) {
	// simulate data
	std::vector<emp::Integer> did_1;
	std::vector<emp::Integer> did_1_b;
	std::vector<emp::Integer> did_2;
	std::vector<emp::Integer> did_2_b;
	std::vector<emp::Integer> lists;
	std::vector<emp::Integer> lists_b;
	std::vector<emp::Bit> confirmed;
	std::vector<emp::Bit> confirmed_b;
	for (size_t i = 0; i < tile_size; i++) {
		did_1.emplace_back(emp::Integer(256, tile_size - i, emp::ALICE));
		did_2.emplace_back(emp::Integer(256, i, emp::ALICE));
		confirmed.emplace_back(emp::Bit(1, emp::ALICE));
	}
	for (size_t i = 0; i < 2*tile_size; i++)
		lists.emplace_back(emp::Integer(32, i, emp::ALICE));
	for (size_t i = 0; i < tile_size; i++) {
		did_1_b.emplace_back(emp::Integer(256, i, emp::BOB));
		did_2_b.emplace_back(emp::Integer(256, tile_size - i, emp::BOB));
		confirmed_b.emplace_back(emp::Bit(0, emp::BOB));
	}
	for (size_t i = 0; i < 2*tile_size; i++)
		lists_b.emplace_back(emp::Integer(32, 0, emp::BOB));
	for (size_t i = 0; i < tile_size; i++) {
		did_1[i] = did_1[i] ^ did_1_b[i];
		did_2[i] = did_2[i] ^ did_2_b[i];
		confirmed[i] = confirmed[i] ^ confirmed_b[i];
	}
	for (size_t i = 0; i < 2*tile_size; i++)
		lists[i] = lists[i] ^ lists_b[i];
	emp::Integer sick = did_1[0];

	// start mapping
	const emp::Integer none(32, -2147483648, emp::PUBLIC);
	const emp::Bit one_bit(1, emp::PUBLIC);

	// first map appending to the lists
	for (size_t i = 0; i < tile_size; i++) {
		lists[output_size+i] = emp::If((did_1[i] == sick) & (confirmed[i] == one_bit),
				(did_2[i]).resize(32), none);
	}

	// sort only the new tile
	emp::sort(&lists[output_size], tile_size, (Bit*)nullptr,
			true);
	// for (size_t j = 0; j < lists.size(); j++) {
	//    std::cout << "Element (total " << lists.size() << ") "
	//    << j <<
	//    ": " << lists[j].reveal<int>() << std::endl;
	//}
	// merge to put duplicates close
	emp::bitonic_merge(&lists[0], (Bit*)nullptr, 0,
			lists.size(), true);
	// for (size_t j = 0; j < lists.size(); j++) {
	//    std::cout << "Merged Element (total " << lists.size()
	//    << ") "
	//    << j << ": " << lists[j].reveal<int>() << std::endl;
	//}
	// mark duplicates, pre-process for compaction
	std::vector<emp::Integer> distance =
		compute_distance_mark_duplicates(&lists[0], lists.size());
	// compact the list pushing duplicates at the end
	compact(distance, &lists[0], lists.size());
}

void test_sort(size_t size = 10000) {
	std::vector<emp::Integer> list;
	std::vector<emp::Integer> list_b;
	for (size_t i = 0; i < size; i++) 
		list.emplace_back(32, size-i, emp::ALICE);	
	for (size_t i = 0; i < size; i++)
		list_b.emplace_back(32, i, emp::BOB);
	for (size_t i = 0; i < size; i++)
		list[i] = list[i] ^ list_b[i];
	emp::sort(&list[0], size);
}

// original emp-tool function
void sort(int n) {
	Integer *A = new Integer[n];
	Integer *B = new Integer[n];
	for(int i = 0; i < n; ++i) {
		A[i] = Integer(32, n - i, ALICE);
	}
	for(int i = 0; i < n; ++i) {
		B[i] = Integer(32, i, BOB);
	}
	for(int i = 0; i < n; ++i)
		A[i] = A[i] ^ B[i];
	sort(A, n);
	for(int i = 0; i < n; ++i)
		A[i].reveal<string>();
}


void generate_data_sort(size_t size = 10000) {
	std::vector<emp::Integer> list;
	std::vector<emp::Integer> list_b;
	for (size_t i = 0; i < size; i++) 
		list.emplace_back(32, size-i, emp::ALICE);	
	for (size_t i = 0; i < size; i++)
		list_b.emplace_back(32, i, emp::BOB);
	for (size_t i = 0; i < size; i++)
		list[i] = list[i] ^ list_b[i];
}

int main(int argc, char* argv[]) {
	std::string filename = "sort_10k";
	setup_plain_prot(true, (filename+".txt").c_str());
	// generate_data_dmr(6000, 500);
	test_sort(10000);
	finalize_plain_prot();
	BristolFormat bf((filename+".txt").c_str());
	bf.to_file((filename+"_file.h").c_str(), filename.c_str());
	return 0;
}
