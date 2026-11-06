// MACS hob controller housing - ESP32 D1 mini (MiniKit) + D1 proto shield stack
// Parametric OpenSCAD model. Set `part` to "base", "lid" or "assembly".
// Print: ASA/ABS preferred, PETG minimum. No supports.
// Base prints open side up; the "lid" part is already flipped outside-face down.

part = "assembly"; // [base, lid, assembly]

/* [Board stack - measure and adjust] */
board_l      = 39.0;  // ESP32 D1 mini PCB length (USB end to antenna end)
board_w      = 31.4;  // ESP32 D1 mini PCB width
pcb_t        = 1.6;
under_clear  = 4.0;   // space under the ESP32 PCB for bottom-side parts
stack_h      = 22.0;  // ESP32 PCB bottom -> highest point on/above the shield
header_gap   = 8.5;   // ESP32 PCB top -> shield PCB bottom
shield_inset = 2.0;   // shield corners inset from the ESP32 PCB edge

/* [Fit] */
clr       = 0.4;   // clearance per side around the PCB
wall      = 2.0;
floor_t   = 2.0;
lid_t     = 2.0;
ledge_w   = 2.0;
ledge_len = 6.0;
lip_h     = 2.0;
lip_t     = 1.2;
lip_clr   = 0.2;   // per side between lid lip and base cavity
post_d    = 3.0;
post_gap  = 0.2;   // post stops this far above the shield (0 = light preload)

/* [Cable exit, -X end] */
cable_w = 8.0;
cable_z = 1.0;     // slot bottom above floor top

/* [USB window, +X end] */
usb_window      = true;
usb_w           = 12.0;
usb_h           = 7.0;
usb_z_above_pcb = -1.0;  // window centre relative to PCB top surface

/* [Screws] */
boss_d          = 6.0;
screw_pilot     = 1.7;   // M2 self-tapping (3.2 for M2 heat-set inserts)
screw_clear     = 2.4;
screw_head_d    = 4.2;
screw_head_depth= 1.2;

/* [Mounting ears] */
ears         = true;
ear_len      = 9.0;
ear_t        = 3.0;
ear_slot_w   = 3.4;
ear_slot_len = 5.0;

/* [Lid vents] */
vents    = true;
vent_w   = 1.6;
vent_len = 16.0;
vent_n   = 5;

/* [Hidden] */
$fn   = 48;
eps   = 0.01;
in_l  = board_l + 2*clr;
in_w  = board_w + 2*clr;
in_h  = under_clear + stack_h;
out_l = in_l + 2*wall;
out_w = in_w + 2*wall;
base_h = floor_t + in_h;
bx = out_l/2 + boss_d/2 - wall;
by = out_w/2 + boss_d/2 - wall;
boss_pts = [for (sx=[-1,1], sy=[-1,1]) [sx*bx, sy*by]];
pcb_top = floor_t + under_clear + pcb_t;
shield_top = pcb_top + header_gap + pcb_t;
post_len = base_h - shield_top - post_gap;

module rounded_box(l, w, h, r=1.5) {
    linear_extrude(h)
        offset(r) square([l-2*r, w-2*r], center=true);
}

module bosses(h) {
    for (p = boss_pts) translate(p) cylinder(d=boss_d, h=h);
}

module base() {
    difference() {
        union() {
            rounded_box(out_l, out_w, base_h);
            bosses(base_h);
            // strain-relief plate outside the cable slot
            translate([-out_l/2 - 3 - 4, -(cable_w+8)/2, 0])
                cube([8, cable_w+8, floor_t]);
            // mounting ears
            if (ears) for (sy=[-1,1])
                translate([0, sy*(out_w/2 + ear_len/2 - 0.5), 0])
                    rounded_box(out_l*0.5, ear_len, ear_t, 1.0);
        }
        // cavity
        translate([-in_l/2, -in_w/2, floor_t]) cube([in_l, in_w, in_h+1]);
        // screw pilots
        for (p = boss_pts) translate([p[0], p[1], floor_t]) cylinder(d=screw_pilot, h=base_h);
        // cable slot, open to the top
        translate([-out_l/2 - wall*1.5, -cable_w/2, floor_t + cable_z])
            cube([wall*3, cable_w, base_h]);
        // zip-tie holes in the strain-relief plate
        for (sy=[-1,1])
            translate([-out_l/2 - 4 - 1, sy*(cable_w/2 + 1.5) - 2, -1])
                cube([2, 4, floor_t+2]);
        // USB window
        if (usb_window)
            translate([out_l/2 - wall*1.5, -usb_w/2, pcb_top + usb_z_above_pcb - usb_h/2])
                cube([wall*3, usb_w, usb_h]);
        // ear slots
        if (ears) for (sy=[-1,1])
            translate([0, sy*(out_w/2 + ear_len/2), -1])
                hull() for (sx=[-1,1])
                    translate([sx*ear_slot_len/2, 0, 0]) cylinder(d=ear_slot_w, h=ear_t+2);
    }
    // corner ledges the PCB rests on
    for (sx=[-1,1], sy=[-1,1]) {
        translate([sx*(in_l/2 - ledge_len/2) - ledge_len/2, sy*(in_w/2 - ledge_w/2) - ledge_w/2, floor_t - eps])
            cube([ledge_len, ledge_w, under_clear + eps]);
        translate([sx*(in_l/2 - ledge_w/2) - ledge_w/2, sy*(in_w/2 - ledge_len/2) - ledge_len/2, floor_t - eps])
            cube([ledge_w, ledge_len, under_clear + eps]);
    }
}

// lid modelled in assembly position: plate z = 0..lid_t, lip and posts below 0
module lid_raw() {
    ll = in_l - 2*lip_clr;
    lw = in_w - 2*lip_clr;
    difference() {
        union() {
            rounded_box(out_l, out_w, lid_t);
            bosses(lid_t);
            // locating lip
            translate([0, 0, -lip_h]) difference() {
                translate([-ll/2, -lw/2, 0]) cube([ll, lw, lip_h + eps]);
                translate([-ll/2 + lip_t, -lw/2 + lip_t, -1]) cube([ll - 2*lip_t, lw - 2*lip_t, lip_h + 2]);
            }
            // hold-down posts on the shield corners
            if (post_len > 0.5) {
                px = in_l/2 - clr - shield_inset - post_d/2;
                py = in_w/2 - clr - shield_inset - post_d/2;
                for (sx=[-1,1], sy=[-1,1])
                    translate([sx*px, sy*py, -post_len]) cylinder(d=post_d, h=post_len + eps);
            }
        }
        // lip relief at the cable slot
        translate([-in_l/2 - 3, -cable_w/2, -lip_h - 1]) cube([6, cable_w, lip_h + 1]);
        // screw clearance and counterbore
        for (p = boss_pts) {
            translate([p[0], p[1], -1]) cylinder(d=screw_clear, h=lid_t + 2);
            translate([p[0], p[1], lid_t - screw_head_depth]) cylinder(d=screw_head_d, h=screw_head_depth + 1);
        }
        // vents
        if (vents) {
            pitch = vent_w * 2.2;
            for (i = [0:vent_n-1])
                translate([-pitch*(vent_n-1)/2 + i*pitch, 0, -1])
                    hull() for (sy=[-1,1])
                        translate([0, sy*(vent_len - vent_w)/2, 0]) cylinder(d=vent_w, h=lid_t + 2);
        }
    }
}

module lid() { // print orientation: outside face down on the bed
    translate([0, 0, lid_t]) rotate([180, 0, 0]) lid_raw();
}

if (part == "base") base();
else if (part == "lid") lid();
else {
    color([0.15, 0.2, 0.35]) base();
    color([0.75, 0.6, 0.4]) translate([0, 0, base_h + 15]) lid_raw();  // exploded
}
