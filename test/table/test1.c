#include "../../src/tomlc17.c"

static void failed() {
  printf("FAILED\n");
  exit(1);
}

#define CHECK(x)                                                               \
  if (x)                                                                       \
    ;                                                                          \
  else                                                                         \
    failed()

// A document of n tables under [t], each [t.kNNNNN] holding v = NNNNN, with
// key repeated again at the end when repeat is set
static char *make_doc(int n, int repeat) {
  char *doc = malloc((size_t)(n + 1) * 32);
  int len = 0;
  for (int i = 0; i < n; i++) {
    len += sprintf(doc + len, "[t.k%05d]\nv = %d\n", i, i);
  }
  if (repeat >= 0) {
    len += sprintf(doc + len, "[t.k%05d]\nv = 0\n", repeat);
  }
  return doc;
}

// Every key of a large table is found, in order, and none that isn't there.
static void test_large_table() {
  printf("Running test_large_table...\n");
  const int n = 10000;
  char *doc = make_doc(n, -1);
  toml_result_t result = toml_parse(doc, strlen(doc));
  CHECK(result.ok);
  toml_datum_t t = toml_get(result.toptab, "t");
  CHECK(t.type == TOML_TABLE && t.u.tab.size == n);
  for (int i = 0; i < n; i++) {
    char key[16];
    sprintf(key, "k%05d", i);
    CHECK(t.u.tab.len[i] == 6 && 0 == memcmp(t.u.tab.key[i], key, 6));
    toml_datum_t v = toml_get(toml_get(t, key), "v");
    CHECK(v.type == TOML_INT64 && v.u.int64 == i);
  }
  CHECK(toml_get(t, "k99999").type == TOML_UNKNOWN);
  CHECK(toml_get(t, "k0000").type == TOML_UNKNOWN);
  toml_free(result);
  free(doc);
}

// A key given twice is refused in a large table as in a small one.
static void test_large_table_duplicate() {
  printf("Running test_large_table_duplicate...\n");
  char *doc = make_doc(10000, 4321);
  toml_result_t result = toml_parse(doc, strlen(doc));
  CHECK(!result.ok);
  toml_free(result);
  free(doc);

  doc = make_doc(10, 3);
  result = toml_parse(doc, strlen(doc));
  CHECK(!result.ok);
  toml_free(result);
  free(doc);
}

// Only tables of TAB_INDEX_MIN keys or more carry an index.
static void test_index_threshold() {
  printf("Running test_index_threshold...\n");
  char *doc = make_doc(TAB_INDEX_MIN - 1, -1);
  toml_result_t result = toml_parse(doc, strlen(doc));
  CHECK(result.ok);
  CHECK(*tab_index(&result.toptab.u.tab.value[0]) == NULL);
  toml_free(result);
  free(doc);

  doc = make_doc(TAB_INDEX_MIN + 1, -1);
  result = toml_parse(doc, strlen(doc));
  CHECK(result.ok);
  CHECK(*tab_index(&result.toptab.u.tab.value[0]) != NULL);
  toml_free(result);
  free(doc);
}

// A large table merged into another keeps every key findable.
static void test_large_table_merge() {
  printf("Running test_large_table_merge...\n");
  char *doc = make_doc(2000, -1);
  toml_result_t base = toml_parse(doc, strlen(doc));
  toml_result_t more = toml_parse("[t.extra]\nv = -1\n", 17);
  CHECK(base.ok && more.ok);
  toml_result_t merged = toml_merge(&base, &more);
  CHECK(merged.ok);
  toml_datum_t t = toml_get(merged.toptab, "t");
  CHECK(t.u.tab.size == 2001);
  CHECK(toml_get(toml_get(t, "k01999"), "v").u.int64 == 1999);
  CHECK(toml_get(toml_get(t, "extra"), "v").u.int64 == -1);
  toml_free(merged);
  toml_free(more);
  toml_free(base);
  free(doc);
}

// Tables and arrays hold more than the 16k they once stopped at.
static void test_past_16k() {
  printf("Running test_past_16k...\n");
  const int n = 20000;
  char *doc = make_doc(n, -1);
  toml_result_t result = toml_parse(doc, strlen(doc));
  CHECK(result.ok);
  CHECK(toml_get(result.toptab, "t").u.tab.size == n);
  toml_free(result);
  free(doc);

  doc = malloc((size_t)n * 8 + 16);
  int len = sprintf(doc, "a = [");
  for (int i = 0; i < n; i++) {
    len += sprintf(doc + len, "%d,", i);
  }
  len += sprintf(doc + len, "]\n");
  result = toml_parse(doc, len);
  CHECK(result.ok);
  toml_datum_t a = toml_get(result.toptab, "a");
  CHECK(a.type == TOML_ARRAY && a.u.arr.size == n);
  CHECK(a.u.arr.elem[n - 1].u.int64 == n - 1);
  toml_free(result);
  free(doc);
}

int main() {
  test_large_table();
  test_past_16k();
  test_large_table_duplicate();
  test_index_threshold();
  test_large_table_merge();
  printf("All tests passed.\n");
  return 0;
}
