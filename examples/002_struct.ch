struct Vec3 = {
  x: float,
  y: float,
  z: float
}

fn length_sq(v: Vec3): float {
  var r: float = v.x * v.x + v.y * v.y + v.z * v.z;
  return r;
}

fn main(): int {
  var v: Vec3;
  
  v.x = 1.0;
  v.y = 2.0;
  v.z = 3.0;

  print(v.x, v.y, v.z);

  var l: float = length_sq(v);
  print(l);

  var i: int = 0;

  while (i < 3) {
    v.x = v.x + 1.0;
    print(v.x);
    i = i + 1;
  }

  return 0;
}
