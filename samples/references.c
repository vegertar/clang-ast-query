void foo(int
n) {
n++;
int
x,
y;

struct bar {
  int
n;
};

struct foo {
  struct bar
bar[4];
};

struct foo
foo;

(void)
foo.
bar[
n].
n;

#define \
 N \
 foo.\
 bar[\
 n-1 + \
 y + \
 x + 1].\
 n
(void)
N;
}
