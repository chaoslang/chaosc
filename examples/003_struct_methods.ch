struct Vec3 = {
  x: float,
  y: float,
  z: float
}

fn Vec3.length(self: Vec3): float {
  var v: Vec3 = self;
  var r: float = v.x * v.x + v.y * v.y + v.z * v.z;
  return r;
}

fn Vec3.print(self: Vec3): void {
  print("Printing Vec3");
  print(self.x, self.y, self.z);
}


fn main(): int {
  var v: Vec3;

  v.x = 1.0;
  v.y = 2.0;
  v.z = 3.0;

  v.print();
  
  var l: float = v.length();
  print(l);

  var i: int = 0;

  while (i < 3) {
    v.x = v.x + 1.0;
    print(v.x);
    i = i + 1;
  }

  return 0;
}
