#![feature(core)]

extern crate core;
use core::intrinsics::volatile_store;

extern {
    static mut bss_start: u8;
    static mut bss_end: u8;
}

unsafe fn byte_ptr<T>(obj: &mut T) -> *mut u8 {
    return obj as *mut T as *mut u8;
}

fn clear_bss() {
    unsafe {
        let start = byte_ptr(&mut bss_start);
        let end = byte_ptr(&mut bss_end);
        let size = end as usize - start as usize;
        for i in 0..size {
            volatile_store(start.offset(i as isize), 0);
        }
    }
}

#[no_mangle]
pub fn kernel_entry() {
    clear_bss();
    loop {}
}
