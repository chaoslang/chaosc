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
  print("Printing Vec3", self.x, self.y, self.z);
}

fn main(): int {
  var v: Vec3;

  v.x = 1.0;
  v.y = 2.0;
  v.z = 3.0;

  v.print();
  print(v.length());

  return 0;
}
