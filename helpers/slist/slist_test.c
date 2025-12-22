
#include <stdio.h>

#include "slist.h"

void print_list(slist_t *list) {
  while (list != NULL) {
    printf("%s\n", list->data);
    list = list->next;
  }
}

#ifdef SLIST_TEST_MAIN
int main(int argc, char **argv) {
  slist_t *list1 =
      slist_cons("This", slist_cons("is", slist_cons("a", slist_cons("list", NULL))));

  printf("List 1:\n");
  print_list(list1);

  // Try splitting
  const char *str =
      "Each|of|these|words|should|be|a|separate|entry|except these three";

  printf("\nExploding \"%s\":\n", str);
  slist_t *list2 = slist_explode(str, '|');

  print_list(list2);

  slist_free(list1);
  slist_free(list2);
  return 0;
}
#endif
