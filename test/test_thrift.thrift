namespace cpp unit;

service TestThrift {
      i32 add(1:i32 a, 2:i32 b);
      string substr(1:string str, 2:i32 idx, 3:i32 length);
};

