char
v1;
signed char
v2;
unsigned
char
v3;
short
v4;
short int
v5;
signed
short
v6;
signed
short int
v7;
unsigned
short
v8;
unsigned
short int
v9;
int
v10;
signed
v11;
signed int
v12;
unsigned
v13;
unsigned
int
v14;
long
v15;
long int
v16;
signed long
v17;
signed long
int
v18;
unsigned
long
v19;
unsigned
long int
v20;
long long
v21;
long long
int
v22;
signed long
long
v23;
signed long
long int
v24;
unsigned
long long
v25;
unsigned
long long
int
v26;
float
v27;
double
v28;
long double
v29;
float
_Complex
v30;
double
_Complex
v31;
long double
_Complex
v32;
_Bool
v33;

typedef
char
t1;
typedef
signed char
t2;
typedef
unsigned
char
t3;
typedef
short
t4;
typedef
short int
t5;
typedef
signed
short
t6;
typedef
signed
short int
t7;
typedef
unsigned
short
t8;
typedef
unsigned
short int
t9;
typedef
int
t10;
typedef
signed
t11;
typedef
signed int
t12;
typedef
unsigned
t13;
typedef
unsigned
int
t14;
typedef
long
t15;
typedef
long int
t16;
typedef
signed long
t17;
typedef
signed long
int
t18;
typedef
unsigned
long
t19;
typedef
unsigned
long int
t20;
typedef
long long
t21;
typedef
long long
int
t22;
typedef
signed long
long
t23;
typedef
signed long
long int
t24;
typedef
unsigned
long long
t25;
typedef
unsigned
long long
int
t26;
typedef
float
t27;
typedef
double
t28;
typedef
long double
t29;
typedef
float
_Complex
t30;
typedef
double
_Complex
t31;
typedef
long double
_Complex
t32;
typedef
_Bool
t33;

// Index both type and variable
t33
v34;

// Index type and its alias
typedef
t33
t34;

// Index all typedef types but parameter variables.
typedef
t34
t35(
t32
v35);

// Without function body, index all typedef types.
t34
v36(
t32
v37);

// With function body, index all typedef types and parameters.
t34
v36(
t32
v37) { return 0; }
