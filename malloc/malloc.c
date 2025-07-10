//
// >>>> malloc challenge! <<<<
//
// Your task is to improve utilization and speed of the following malloc
// implementation.
// Initial implementation is the same as the one implemented in simple_malloc.c.
// For the detailed explanation, please refer to simple_malloc.c.

#include <assert.h> // バグ防止
#include <stdbool.h> // boolを使えるように
#include <stddef.h> 
#include <stdint.h> // 指定された幅を持つ整数型のセットを宣言しマクロの集合を定義
#include <stdio.h> // 標準入出力
#include <stdlib.h> 
#include <string.h>

//
// Interfaces to get memory pages from OS
//

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

//
// Struct definitions
//

typedef struct my_metadata_t { // 空き領域を管理するための構造体
  size_t size; // データ部分のサイズを格納
  struct my_metadata_t *next; // 次の空きブロックを指すポインタ(単方向リンクリスト)
} my_metadata_t;

typedef struct my_heap_t { // ヒープ全体の管理
  my_metadata_t *free_head; // 現在の空き領域リストの先頭を指すポインタ
  my_metadata_t dummy; // ダミーとして使う空きブロックのメタデータ
} my_heap_t;

//
// Static variables (DO NOT ADD ANOTHER STATIC VARIABLES!)
//
my_heap_t my_heap;

//
// Helper functions (feel free to add/remove/edit!)
//

void my_add_to_free_list(my_metadata_t *metadata) { // 渡されたmetadataをfree listの先頭に追加する関数
  assert(!metadata->next); // 別のノードを指していないことを確認
  metadata->next = my_heap.free_head; // 現在のリストの先頭をmetadataのnextに設定する
  my_heap.free_head = metadata; // free_headを新しいノードに更新→リストの先頭が変わる
}

void my_remove_from_free_list(my_metadata_t *metadata, my_metadata_t *prev) { // metadataをfree listから削除
  if (prev) { // metadataがリストの途中にあるとき
    prev->next = metadata->next; // 前のノードのnextをmetadataのnextに更新→metadataを飛ばす
  } else { // metadataが先頭ノードのとき
    my_heap.free_head = metadata->next; // free listの先頭をmetadataのnextに更新
  }
  metadata->next = NULL; // 念の為？
}

//
// Interfaces of malloc (DO NOT RENAME FOLLOWING FUNCTIONS!)
//

// This is called at the beginning of each challenge.
void my_initialize() { // 初期化処理
  my_heap.free_head = &my_heap.dummy; // free_headをダミー領域に設定
  my_heap.dummy.size = 0;
  my_heap.dummy.next = NULL;
}

// my_malloc() is called every time an object is allocated.
// |size| is guaranteed to be a multiple of 8 bytes and meets 8 <= |size| <=
// 4000. You are not allowed to use any library functions other than
// mmap_from_system() / munmap_to_system().
void *my_malloc(size_t size) { 
  my_metadata_t *metadata = my_heap.free_head; // free listの先頭ノード
  my_metadata_t *prev = NULL; // metadataの直前のノード
  my_metadata_t *best_fit = NULL; // best_fitの空きブロック
  my_metadata_t *best_fit_prev = NULL; // best_fitの直前のノード

  // First-fit: Find the first free slot the object fits.
  // TODO: Update this logic to Best-fit!
  // best-fit: 使える最小の空き領域を探す
  while (metadata) {
    if (metadata->size >= size) { // 必要サイズ以上の空き領域なら
      if (!best_fit || metadata->size < best_fit->size) {
        best_fit = metadata; // より小さいサイズの空き領域を更新
        best_fit_prev = prev;
      }
    }
    prev = metadata; // 使用する空きブロックを確定
    metadata = metadata->next;
  }

  metadata = best_fit;
  prev = best_fit_prev;
  // now, metadata points to the first free slot
  // and prev is the previous entry.

  // 空きブロックがなかった場合、新たにメモリを確保する
  if (!metadata) {
    // There was no free slot available. We need to request a new memory region
    // from the system by calling mmap_from_system().
    //
    //     | metadata | free slot |
    //     ^
    //     metadata
    //     <---------------------->
    //            buffer_size
    size_t buffer_size = 4096; // OSから確保する単位
    my_metadata_t *metadata = (my_metadata_t *)mmap_from_system(buffer_size); // OSからメモリ取得
    metadata->size = buffer_size - sizeof(my_metadata_t); // ユーザが使えるサイズを設定
    metadata->next = NULL;
    
    my_add_to_free_list(metadata); // free listに追加
    return my_malloc(size); // 再帰呼び出し
  }

  // |ptr| is the beginning of the allocated object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  void *ptr = metadata + 1; // ユーザが使うメモリ領域（metadataの直後）
  size_t remaining_size = metadata->size - size; // 残りのサイズを計算する

  my_remove_from_free_list(metadata, prev); // 使用するブロックをfree listから削除

  if (remaining_size > sizeof(my_metadata_t)) { // 残りが新たなブロックとして使えるなら
    // Shrink the metadata for the allocated object
    // to separate the rest of the region corresponding to remaining_size.
    // If the remaining_size is not large enough to make a new metadata,
    // this code path will not be taken and the region will be managed
    // as a part of the allocated object.
    metadata->size = size; // 使用ブロックのサイズを確定（縮小する）
    // Create a new metadata for the remaining free slot.
    //
    // ... | metadata | object | metadata | free slot | ...
    //     ^          ^        ^
    //     metadata   ptr      new_metadata
    //                 <------><---------------------->
    //                   size       remaining size
    my_metadata_t *new_metadata = (my_metadata_t *)((char *)ptr + size); //残り領域の先頭に新しいメタデータを作成する
    new_metadata->size = remaining_size - sizeof(my_metadata_t); // 残りのサイズからメタデータのサイズをひく
    new_metadata->next = NULL; 

    my_add_to_free_list(new_metadata); // 残り領域をfree listに追加する」
  }
  return ptr; // ポインタを返す
}

// This is called every time an object is freed.  You are not allowed to
// use any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) {
  // Look up the metadata. The metadata is placed just prior to the object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  my_metadata_t *metadata = (my_metadata_t *)ptr - 1;
  // Add the free slot to the free list.
  my_add_to_free_list(metadata);
}

// This is called at the end of each challenge.
void my_finalize() {
  // Nothing is here for now.
  // feel free to add something if you want!
}

void test() {
  // Implement here!
  assert(1 == 1); /* 1 is 1. That's always true! (You can remove this.) */
}
