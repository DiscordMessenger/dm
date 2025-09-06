/***
	The Boron Operating System
	Copyright (C) 2023 iProgramInCpp

Module name:
	rtl/list.h
	
Abstract:
	This header file contains the definitions related
	to the linked list code.
	
	It is important to note that the list entries' memory
	is managed manually by the user of these functions.
	This is merely the implementation of the doubly circularly
	linked list data structure.
	
Author:
	iProgramInCpp - 2 October 2023
***/
#ifndef BORON_RTL_LIST_H
#define BORON_RTL_LIST_H

//#include <main.h>
#include <stdint.h>

#ifdef MINGW_SPECIFIC_HACKS
#define ALWAYS_INLINE __attribute__((always_inline))
#define UNUSED __attribute__((unused))
#else
#define ALWAYS_INLINE
#define UNUSED
#endif

#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(Pointer, Type, Field) ((Type*)((uintptr_t)(Pointer) - (uintptr_t)offsetof(Type, Field)))
#endif

typedef struct _BLIST_ENTRY BLIST_ENTRY, *PBLIST_ENTRY;

struct _BLIST_ENTRY
{
	PBLIST_ENTRY Flink;
	PBLIST_ENTRY Blink;
};

// void BInitializeListHead(PBLIST_ENTRY ListHead);
#define BInitializeListHead(Head) \
	(Head)->Flink = (Head),      \
	(Head)->Blink = (Head)

// void BInsertHeadList(PBLIST_ENTRY ListHead, PBLIST_ENTRY Entry);
#define BInsertHeadList(ListHead, Entry) \
    (Entry)->Flink = (ListHead)->Flink,   \
	(Entry)->Blink = (ListHead),          \
	(ListHead)->Flink->Blink = (Entry), \
	(ListHead)->Flink = (Entry)

// void BInsertTailList(PBLIST_ENTRY ListHead, PBLIST_ENTRY Entry);
#define BInsertTailList(ListHead, Entry) \
    (Entry)->Blink = (ListHead)->Blink,   \
	(Entry)->Flink = (ListHead),          \
	(ListHead)->Blink->Flink = (Entry), \
	(ListHead)->Blink = (Entry)

// PBLIST_ENTRY BRemoveHeadList(PBLIST_ENTRY ListHead);
static inline ALWAYS_INLINE UNUSED
PBLIST_ENTRY BRemoveHeadList(PBLIST_ENTRY ListHead)
{
	PBLIST_ENTRY Entry = ListHead->Flink;
	ListHead->Flink = Entry->Flink;
	ListHead->Flink->Blink = ListHead;
#ifdef DEBUG
	Entry->Flink = NULL;
	Entry->Blink = NULL;
#endif
	return Entry;
}

// PBLIST_ENTRY BRemoveTailList(PBLIST_ENTRY ListHead);
static inline ALWAYS_INLINE UNUSED
PBLIST_ENTRY BRemoveTailList(PBLIST_ENTRY ListHead)
{
	PBLIST_ENTRY Entry = ListHead->Blink;
	ListHead->Blink = Entry->Blink;
	ListHead->Blink->Flink = ListHead;
#ifdef DEBUG
	Entry->Flink = NULL;
	Entry->Blink = NULL;
#endif
	return Entry;
}

// bool BRemoveEntryList(PBLIST_ENTRY Entry);
static inline ALWAYS_INLINE UNUSED
bool BRemoveEntryList(PBLIST_ENTRY Entry)
{
	PBLIST_ENTRY OldFlink, OldBlink;
	OldFlink = Entry->Flink;
	OldBlink = Entry->Blink;
	OldBlink->Flink = OldFlink;
	OldFlink->Blink = OldBlink;
#ifdef DEBUG
	Entry->Flink = NULL;
	Entry->Blink = NULL;
#endif
	return OldFlink == OldBlink;
}

// bool BIsListEmpty(PBLIST_ENTRY ListHead);
#define BIsListEmpty(ListHead) ((ListHead)->Flink == (ListHead))

#endif//BORON_RTL_LIST_H
