#!/usr/bin/env python3

mcu_dict = {
    52832: {
        'flash_size': 290816,
        'data_size': 51712,
        'extra_flags': '-DNRF52832_XXAA -DNRF52',
        'ldscript': 'nrf52832_s132_v6.ld',
        'uf2_family': 0x00
    },
    52833: {
        'flash_size': 286720,
        'data_size': 106496,
        'extra_flags': '-DNRF52833_XXAA {build.flags.usb}',
        'ldscript': 'nrf52833_s140_v7.ld',
        'uf2_family': '0x621E937A'
    },
    52840: {
        'flash_size': 815104,
        'data_size': 237568,
        'extra_flags': '-DNRF52840_XXAA {build.flags.usb}',
        'ldscript': 'nrf52840_s140_v6.ld',
        'uf2_family': '0xADA52840'
    }
}


def build_upload(mcu, name):
    print("# Upload")
    print(f"{name}.bootloader.tool=bootburn")
    print(f"{name}.upload.tool=nrfutil")
    print(f"{name}.upload.protocol=nrfutil")
    if mcu == 52832:
        print(f"{name}.upload.use_1200bps_touch=false")
        print(f"{name}.upload.wait_for_upload_port=false")
        print(f"{name}.upload.native_usb=false")
    else:
        print(f"{name}.upload.use_1200bps_touch=true")
        print(f"{name}.upload.wait_for_upload_port=true")
    print(f"{name}.upload.maximum_size={mcu_dict[mcu]['flash_size']}")
    print(f"{name}.upload.maximum_data_size={mcu_dict[mcu]['data_size']}")
    print()


def build_header(mcu, name, variant, vendor_name, product_name, boarddefine, vid, pid_list):
    prettyname = vendor_name + " " + product_name
    print()
    print("# -----------------------------------")
    print(f"# {prettyname}")
    print("# -----------------------------------")
    print(f"{name}.name={prettyname}")
    print()

    print("# VID/PID for Bootloader, Arduino & CircuitPython")
    for i in range(len(pid_list)):
        print(f"{name}.vid.{i}={vid}")
        print(f"{name}.pid.{i}={pid_list[i]}")
    print()

    build_upload(mcu, name)

    print("# Build")
    print(f"{name}.build.mcu=cortex-m4")
    print(f"{name}.build.f_cpu=64000000")
    print(f"{name}.build.board={boarddefine}")
    print(f"{name}.build.core=nRF5")
    print(f"{name}.build.variant={variant}")
    print(f'{name}.build.usb_manufacturer="{vendor_name}"')
    print(f'{name}.build.usb_product="{product_name}"')

    mcu_info = mcu_dict[mcu]
    print(f"{name}.build.extra_flags={mcu_info['extra_flags']}")
    print(f"{name}.build.ldscript={mcu_info['ldscript']}")
    print(f"{name}.build.openocdscript=scripts/openocd/daplink_nrf52.cfg")
    if mcu != 52832:
        print(f"{name}.build.vid={vid}")
        print(f"{name}.build.pid={pid_list[0]}")
        print(f"{name}.build.uf2_family={mcu_info['uf2_family']}")
    print()


def build_softdevice(mcu, name):
    print("# Menu: SoftDevice")
    if mcu == 52832:
        print(f"{name}.menu.softdevice.s132v6=S132 6.1.1")
        print(f"{name}.menu.softdevice.s132v6.build.sd_name=s132")
        print(f"{name}.menu.softdevice.s132v6.build.sd_version=6.1.1")
        print(f"{name}.menu.softdevice.s132v6.build.sd_fwid=0x00B7")
    elif mcu == 52833:
        print(f"{name}.menu.softdevice.s140v7=S140 7.3.0")
        print(f"{name}.menu.softdevice.s140v7.build.sd_name=s140")
        print(f"{name}.menu.softdevice.s140v7.build.sd_version=7.3.0")
        print(f"{name}.menu.softdevice.s140v7.build.sd_fwid=0x0123")
    elif mcu == 52840:
        print(f"{name}.menu.softdevice.s140v6=S140 6.1.1")
        print(f"{name}.menu.softdevice.s140v6.build.sd_name=s140")
        print(f"{name}.menu.softdevice.s140v6.build.sd_version=6.1.1")
        print(f"{name}.menu.softdevice.s140v6.build.sd_fwid=0x00B6")
    print()


def build_debug(name):
    print("# Menu: Debug Level")
    print(f"{name}.menu.debug.l0=Level 0 (Release)")
    print(f"{name}.menu.debug.l0.build.debug_flags=-DCFG_DEBUG=0")
    print(f"{name}.menu.debug.l1=Level 1 (Error Message)")
    print(f"{name}.menu.debug.l1.build.debug_flags=-DCFG_DEBUG=1")
    print(f"{name}.menu.debug.l2=Level 2 (Full Debug)")
    print(f"{name}.menu.debug.l2.build.debug_flags=-DCFG_DEBUG=2")
    print(f"{name}.menu.debug.l3=Level 3 (Segger SystemView)")
    print(f"{name}.menu.debug.l3.build.debug_flags=-DCFG_DEBUG=3")
    print(f"{name}.menu.debug.l3.build.sysview_flags=-DCFG_SYSVIEW=1")
    print()


def build_debug_output(name):
    print("# Menu: Debug Port")
    print(f"{name}.menu.debug_output.serial=Serial")
    print(f"{name}.menu.debug_output.serial.build.logger_flags=-DCFG_LOGGER=0")
    print(f"{name}.menu.debug_output.serial1=Serial1")
    print(f"{name}.menu.debug_output.serial1.build.logger_flags=-DCFG_LOGGER=1 -DCFG_TUSB_DEBUG=CFG_DEBUG")
    print(f"{name}.menu.debug_output.rtt=Segger RTT")
    print(f"{name}.menu.debug_output.rtt.build.logger_flags=-DCFG_LOGGER=2 -DCFG_TUSB_DEBUG=CFG_DEBUG -DSEGGER_RTT_MODE_DEFAULT=SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL")


def build_global_menu():
    print("menu.softdevice=SoftDevice")
    print("menu.debug=Debug Level")
    print("menu.debug_output=Debug Port")


def make_board(mcu, name, variant, vendor_name, product_name, boarddefine, vid, pid_list):
    build_header(mcu, name, variant, vendor_name, product_name, boarddefine, vid, pid_list)
    build_softdevice(mcu, name)
    build_debug(name)
    build_debug_output(name)


# ------------------------------
# main
# ------------------------------
build_global_menu()

boards_list = [

    [
        52840, "supermini",
        "SuperMini_nRF52840", "Generic",
        "SuperMini nRF52840", "NRF52840_SUPERMINI",
        "0x1209", ["0x5285","0x5284","0x5284","0x5284"]
    ],
    [
        52840, "nrfmicro",
        "nRFMicro", "joric",
        "nRFMicro", "NRF52840_NRFMICRO",
        "0x1209", ["0x5285","0x5284","0x5284","0x5284"]
    ],
    [
        52840, "nicenanov2",
        "nice_nano_v2", "nice!",
        "nice!nano v2", "NRF52840_NICENANO_V2",
        "0x239A", ["0x00B4","0x00B3","0x00B3","0x00B3"]
    ],
]

for b in boards_list:
    make_board(*b)
