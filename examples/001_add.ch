fn add(x: int, y: int): int {
  var z: int = x + y;
  return z;
}

fn main(): int {
  add(5, 3);

  var o: int = 3;
  var t: float = 1.021;
  
  while (o < 10){
    o = o + 1;
    print(o);
  }
  print(add(1, 7), t);
  return 0;
}

