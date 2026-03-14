enum Color = {
  Red,
  Green,
  Blue
}

fn print_color(c: Color): void {
  if (c == 0) {
    print("Red");
  } else {
    if (c == 1) {
      print("Green");
    } else {
      print("Blue");
    }
  }
}

fn main(): int {

  print("Setting to Green!");
  var c: Color = 1;
  print_color(c);

  print("Setting to Blue!");
  c = 2;
  print_color(c);

  return 0;
}
