Context
  setup() {
    db=$(mktemp /tmp/clang-ast-query-db.XXXXXX)
    ./caq -o $db samples/declarations.c
  }
  cleanup() { rm $db; }
  BeforeAll 'setup'
  AfterAll 'cleanup'

  query_decl_number() {
    echo SELECT decl FROM tok WHERE begin_row = $1 AND begin_col = $2
  }

  Describe 'VarDecl with built-in types'
    Parameters
      v1 2 1 "char|v1"
      v2 4 1 "signed char|v2"
      v3 7 1 "unsigned char|v3"
      v4 9 1 "short|v4"
      v5 11 1 "short|v5"
      v6 14 1 "short|v6"
      v7 17 1 "short|v7"
      v8 20 1 "unsigned short|v8"
      v9 23 1 "unsigned short|v9"
      v10 25 1 "int|v10"
      v11 27 1 "int|v11"
      v12 29 1 "int|v12"
      v13 31 1 "unsigned int|v13"
      v14 34 1 "unsigned int|v14"
      v15 36 1 "long|v15"
      v16 38 1 "long|v16"
      v17 40 1 "long|v17"
      v18 43 1 "long|v18"
      v19 46 1 "unsigned long|v19"
      v20 49 1 "unsigned long|v20"
      v21 51 1 "long long|v21"
      v22 54 1 "long long|v22"
      v23 57 1 "long long|v23"
      v24 60 1 "long long|v24"
      v25 63 1 "unsigned long long|v25"
      v26 67 1 "unsigned long long|v26"
      v27 69 1 "float|v27"
      v28 71 1 "double|v28"
      v29 73 1 "long double|v29"
      v30 76 1 "_Complex float|v30"
      v31 79 1 "_Complex double|v31"
      v32 82 1 "_Complex long double|v32"
      v33 84 1 "_Bool|v33"
    End

    query_var_decl() {
      echo SELECT 'qualified_type,name' FROM ast WHERE 'number =' $1
    }

    query() {
      decl_number=$(sqlite3 $db "`query_decl_number $1 $2`")
      sqlite3 $db "`query_var_decl ${decl_number}`"
    }

    It "queries the variable $1"
      When call query "$2" "$3"
      The output should eq "$4"
    End
  End

  Describe 'TypedefDecl with built-in types'
    Parameters
      t1 88 1 "char|t1"
      t2 91 1 "signed char|t2"
      t3 95 1 "unsigned char|t3"
      t4 98 1 "short|t4"
      t5 101 1 "short|t5"
      t6 105 1 "short|t6"
      t7 109 1 "short|t7"
      t8 113 1 "unsigned short|t8"
      t9 117 1 "unsigned short|t9"
      t10 120 1 "int|t10"
      t11 123 1 "int|t11"
      t12 126 1 "int|t12"
      t13 129 1 "unsigned int|t13"
      t14 133 1 "unsigned int|t14"
      t15 136 1 "long|t15"
      t16 139 1 "long|t16"
      t17 142 1 "long|t17"
      t18 146 1 "long|t18"
      t19 150 1 "unsigned long|t19"
      t20 154 1 "unsigned long|t20"
      t21 157 1 "long long|t21"
      t22 161 1 "long long|t22"
      t23 165 1 "long long|t23"
      t24 169 1 "long long|t24"
      t25 173 1 "unsigned long long|t25"
      t26 178 1 "unsigned long long|t26"
      t27 181 1 "float|t27"
      t28 184 1 "double|t28"
      t29 187 1 "long double|t29"
      t30 191 1 "_Complex float|t30"
      t31 195 1 "_Complex double|t31"
      t32 199 1 "_Complex long double|t32"
      t33 202 1 "_Bool|t33"
    End

    query_typedef_decl() {
      echo SELECT 'qualified_type,name' FROM ast WHERE 'number =' $1
    }

    query() {
      decl_number=$(sqlite3 $db "`query_decl_number $1 $2`")
      sqlite3 $db "`query_typedef_decl ${decl_number}`"
    }

    It "queries the typedef $1"
      When call query "$2" "$3"
      The output should eq "$4"
    End
  End
End
