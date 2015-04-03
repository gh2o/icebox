#![feature(core)]

extern crate core;

extern {
    static mut bss_start: u8;
    static mut bss_end: u8;
}

unsafe fn byte_ptr<T>(obj: &mut T) -> *mut u8 {
    return obj as *mut T as *mut u8;
}

unsafe fn clear_bss() {
    let start = byte_ptr(&mut bss_start);
    let end = byte_ptr(&mut bss_end);
    let size = end as usize - start as usize;
    for i in 0..size {
        *start.offset(i as isize) = 0;
    }
}

#[no_mangle]
pub unsafe extern fn kernel_entry() {
    clear_bss();
    loop {}
}
