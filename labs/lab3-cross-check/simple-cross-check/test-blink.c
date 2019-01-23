#include "rpi.h"
#include "simple-check.h"
#include <assert.h>

int main() {
	// exmaple of how to call function once.
  //	xc_run_fn_vv_once("notmain", notmain);

	// example of how to call gpio(pin) function
	xc_run_fn_iu("gpio_set_output", gpio_set_output, xc_get_pin_gen());


}
