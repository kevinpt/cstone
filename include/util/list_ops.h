/* SPDX-License-Identifier: MIT
Copyright 2021 Kevin Thibedeau
(kevin 'period' thibedeau 'at' gmail 'punto' com)

See LICENSE for details
*/

/*
------------------------------------------------------------------------------
  Linked list operations
------------------------------------------------------------------------------
*/

#ifndef LIST_OPS_H
#define LIST_OPS_H

typedef struct NodeSList {
  struct NodeSList *next;
} NodeSList;


typedef struct NodeList {
  struct NodeList *next;
  struct NodeList *prev;
} NodeList;


#if defined(LL_DEBUG) && LL_DEBUG == 1
#  include <stdio.h>
#  define REPORT(msg) fprintf(stderr, "LIST ERROR: %s\n", msg);
#else
#  define REPORT(msg)
#endif

// Convert pointer to NodeSList or NodeList back to address of containing struct
#define LL_NODE(ptr, type, member)  ((type *)((char *)(ptr) - offsetof(type, member)))

// ******************** Singly-linked lists ********************

#define ll_slist_push(head, node) ll__slist_push((NodeSList **)head, (NodeSList *)node)
static inline void ll__slist_push(NodeSList **head, NodeSList *node) {
  node->next = *head; // [[head]]->[x]  ==>  [[node]]->[(old) head]->[x]
  *head = node;
}

#define ll_slist_pop(head)  ll__slist_pop((NodeSList **)head)
static inline NodeSList *ll__slist_pop(NodeSList **head) {
  NodeSList *node = *head;
  if(!node) return NULL;

  *head = node->next;  // [[head]]->[x]->[y]  ==>  [[x]]->[y]
  node->next = NULL;

  return node;
}

#define ll_slist_find(head, node) ll__slist_find((NodeSList *)head, (NodeSList *)node)
static inline bool ll__slist_find(NodeSList *head, NodeSList *node) {
  NodeSList *cur = head;
  while(cur && cur != node) {
    cur = cur->next;
  }

  return cur == node;
}

#define ll_slist_find_prev(head, node) ll__slist_find_prev((NodeSList *)head, (NodeSList *)node)
static inline NodeSList *ll__slist_find_prev(NodeSList *head, NodeSList *node) {
  NodeSList *cur = head;
  while(cur && cur->next != node) {
    cur = cur->next;
  }

  return cur;
}

#define ll_slist_remove(head, node) ll__slist_remove((NodeSList **)head, (NodeSList *)node)
static inline bool ll__slist_remove(NodeSList **head, NodeSList *node) {
  if(node == *head) { // [[node]]->[x]  ==> [[x]]
    ll__slist_pop(head);
  } else {
    NodeSList *prev = ll__slist_find_prev(*head, node);
    if(prev) { // [prev]->[node]->[x]  ==> [prev]->[x]
      prev->next = node->next;
      node->next = NULL;
    } else {
      REPORT("Node not in list");
      return false;
    }
  }

  return true;
}

#define  ll_slist_remove_after(before) ll__slist_remove_after((NodeSList *)before)
static inline  NodeSList *ll__slist_remove_after(NodeSList *before) {
  NodeSList *node = before->next;
  if(node) {
    before->next = node->next;
    node->next = NULL;
  }

  return node;
}

#define ll_slist_add_after(before, node) ll__slist_add_after((NodeSList **)before, (NodeSList *)node)
static inline void ll__slist_add_after(NodeSList **before, NodeSList *node) {
  if(*before) { // [before]->[x]->[y]  ==>  [before]->[node]->[x]->[y]
   node->next = (*before)->next;
   (*before)->next = node;
  } else { // Empty list
    ll__slist_push(before, node);
  }
}

#define ll_slist_add_before(head, after, node) ll__slist_add_before((NodeSList **)head, \
                                                           (NodeSList *)after, (NodeSList *)node)
static inline void ll__slist_add_before(NodeSList **head, NodeSList *after, NodeSList *node) {
  if(*head && after != *head) {
    NodeSList *before = ll__slist_find_prev(*head, after);
    if(before) { // [before]->[after]->[y]  ==>  [before]->[node]->[after]->[y]
      before->next = node;
      node->next = after;
    } else {
      REPORT("After node not in list");
    }
  } else { // Empty list or after is head
    ll__slist_push(head, node);
  }
}


// ******************** Doubly-linked lists ********************

#define ll_list_push(head, node) ll__list_push((NodeList **)head, (NodeList *)node)
static inline void ll__list_push(NodeList **head, NodeList *node) {
  node->next = *head;
  node->prev = NULL;
  *head = node;
}


#define ll_list_pop(head)  ll__list_pop((NodeList **)head)
static inline NodeList *ll__list_pop(NodeList **head) {
  NodeList *node = *head;
  if(!node) return NULL;

  *head = node->next;
  if(*head)
    (*head)->prev = NULL;
  node->next = NULL;

  return node;
}


#define ll_list_find(head, node) ll__list_find((NodeList *)head, (NodeList *)node)
static inline bool ll__list_find(NodeList *head, NodeList *node) {
  NodeList *cur = head;
  while(cur && cur != node) {
    cur = cur->next;
  }

  return cur == node;
}


#define  ll_list_remove_after(before, node) ll__list_remove_after((NodeList *)before)
static inline NodeList *ll__list_remove_after(NodeList *before) {
  NodeList *node = before->next;

  if(node) {
    before->next = node->next;
    if(node->next)
      node->next->prev = before;
    node->next = NULL;
    node->prev = NULL;
  }

  return node;
}


#define ll_list_remove(head, node) ll__list_remove((NodeList **)head, (NodeList *)node)
static inline void ll__list_remove(NodeList **head, NodeList *node) {
  if(node == *head) {
    ll__list_pop(head);
  } else {
    NodeList *prev = node->prev;
    if(prev) {
      prev->next = node->next;
      if(prev->next)
        prev->next->prev = prev;
      node->next = NULL;
      node->prev = NULL;
    } else {
      REPORT("Node not in list");
    }
  }
}


#define ll_list_add_after(before, node) ll__list_add_after((NodeList **)before, (NodeList *)node)
static inline void ll__list_add_after(NodeList **before, NodeList *node) {
  if(*before) {
    NodeList *after = (*before)->next;
    node->next = after;
    node->prev = *before;
    if(after)
      after->prev = node;
    (*before)->next = node;
  } else { // Empty list
    ll__list_push(before, node);
  }
}


#define ll_list_add_before(head, after, node) ll__list_add_before((NodeList **)head, \
                                                           (NodeList *)after, (NodeList *)node)
static inline void ll__list_add_before(NodeList **head, NodeList *after, NodeList *node) {
  if(*head && after != *head) {
    NodeList *before = after->prev;
    if(before) {
      before->next = node;
      node->next = after;
      node->prev = before;
      after->prev = node;
    } else {
      REPORT("After node not in list");
    }
  } else { // Empty list or after is head
    ll__list_push(head, node);
  }
}



// ******************** Circular linked lists ********************

// FIXME: This skips head node
#define ll__clist_foreach(it, head) \
  for(it = (*(head))->next; it != *(head); it = it->next)


#define LL_CLIST_INIT(node)   {.next = &(node), .prev = &(node)}

#define ll_clist_init(node) ll__clist_init((NodeList *)node)
static inline void ll__clist_init(NodeList *node) {
  node->next = node;
  node->prev = node;
}


#define ll_clist_remove(head, node) ll__clist_remove((NodeList **)head, (NodeList *)node)
static inline void ll__clist_remove(NodeList **head, NodeList *node) {
  NodeList *before = node->prev;

  if(before == node) {// Only node in list
    *head = NULL;
  } else if(*head == node) {
    *head = node->next;
  }

  before->next = node->next;
  before->next->prev = before;

  node->next = node;
  node->prev = node;
}


#define ll_clist_add_after(before, node) ll__clist_add_after((NodeList **)before, (NodeList *)node)
static inline void ll__clist_add_after(NodeList *before, NodeList *node) {
  NodeList *after = before->next;
  node->next = after;
  node->prev = before;
  after->prev = node;
  before->next = node;
}


#define ll_clist_add_before(after, node) ll__clist_add_before((NodeList *)after, (NodeList *)node)
static inline void ll__clist_add_before(NodeList *after, NodeList *node) {
  NodeList *before = after->prev;
  node->next = after;
  node->prev = before;
  after->prev = node;
  before->next = node;
}


#define ll_clist_push(head, node) ll__clist_push((NodeList **)head, (NodeList *)node)
static inline void ll__clist_push(NodeList **head, NodeList *node) {
  if(!*head) {
    *head = node;
  } else {
    ll__clist_add_before(*head, node);
    *head = node;
  }
}


#define ll_clist_pop(head)  ll__clist_pop((NodeList **)head)
static inline NodeList *ll__clist_pop(NodeList **head) {
  NodeList *node = *head;

  if(node) {
    *head = node->next;
    ll__clist_remove(head, node);
  }
  return node;

}


#define ll_clist_push_back(head, node)  ll__clist_push_back((NodeList **)head, (NodeList *)node)
static inline void ll__clist_push_back(NodeList **head, NodeList *node) {
  if(!*head) {
    *head = node;
  } else {
    ll__clist_add_before(*head, node);
  }
}


#define ll_clist_pop_back(head)  ll__clist_pop_back((NodeList **)head)
static inline NodeList *ll__clist_pop_back(NodeList **head) {
  if(!*head) return NULL;
  NodeList *node = (*head)->prev;

  *head = node->next;
  ll__clist_remove(head, node);

  return node;
}

#endif // LIST_OPS_H

