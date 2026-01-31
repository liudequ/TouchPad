// Touchpad open-top case model (units: mm)
// Dimensions per docs/case_notes.md

$fn = 64;

// Inner cavity (fits touchpad)
inner_length = 111;
inner_width  = 63;
inner_height = 9.5;

wall_thickness  = 2;
bottom_thickness = 2;

// USB opening on front wall (long side)
usb_width  = 7.8; // horizontal (X)
usb_height = 3;   // vertical (Z)

outer_length = inner_length + 2 * wall_thickness;
outer_width  = inner_width  + 2 * wall_thickness;
outer_height = inner_height + bottom_thickness; // open top
fillet_radius = 2; // outer vertical edge fillet

module case_body() {
  // Outer shell with filleted vertical edges
  linear_extrude(height = outer_height) {
    offset(r = fillet_radius)
      offset(r = -fillet_radius)
        square([outer_length, outer_width], center = false);
  }
}

module inner_cavity() {
  // Subtract inner cavity (open top)
  translate([wall_thickness, wall_thickness, bottom_thickness]) {
    cube([inner_length, inner_width, inner_height], center = false);
  }
}

module usb_opening() {
  // Front wall is the long side at Y=0
  usb_x = outer_length / 2 - usb_width / 2;
  usb_y = -0.1; // ensure full cut through wall
  usb_z = bottom_thickness; // bottom edge flush with inner floor
  translate([usb_x, usb_y, usb_z]) {
    cube([usb_width, wall_thickness + 0.2, usb_height], center = false);
  }
}

difference() {
  case_body();
  inner_cavity();
  usb_opening();
}
