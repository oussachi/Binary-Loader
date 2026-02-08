#include <bfd.h>
#include "loader.h"

int load_binary(std::string &fname, Binary *bin, Binary::BinaryType type) {
    return load_binary_bfd(fname, bin, type);
}

void unload_binary(Binary *bin) {
    size_t i;
    Section *sec;

    for(i = 0; i < bin->sections.size(); i++) {
        sec = &bin->sections[i];
        if(sec->bytes) {
            free(sec->bytes);
        }
    }
}

// Used to open a binary
static bfd* open_bfd(std::string &fname) {
    static int bfd_inited = 0;
    bfd *bfd_h;

    if(!bfd_inited) {
        // A function used to initialize bfd's "magical" internal data structure
        // This initialization is tracked with the variable bfd_inited
        bfd_init();
        bfd_inited = 1;
    }

    /*A function used to open the binary file by name, returns a pointer to the file handle of type
    bfd which is libbfd's root data structure */
    bfd_h = bfd_openr(fname.c_str(), NULL);

    if(!bfd_h) {
        fprintf(stderr, "failed to open binary '%s' (%s)\n",
        /*bfd_get_error() gets the most recent error and returns a bfd_error_type object, which is 
        translated to a string by bfd_errmsg()*/
        fname.c_str(), bfd_errmsg(bfd_get_error()));
        return NULL;
    }

    /*bfd_check_format() checks the format of the given file -object, core or archive- 
    a bfd_object is either a binary, relocatable object or shared library*/
    if(!bfd_check_format(bfd_h, bfd_object)) {
        fprintf(stderr, "file '%s' does not look like an executable (%s)\n",
        fname.c_str(), bfd_errmsg(bfd_get_error()));
        return NULL;
    }

    /* Some versions of bfd_check_format pessimistically set a wrong_format
    * error before detecting the format and then neglect to unset it once
    * the format has been detected. We unset it manually to prevent problems.
    */

    bfd_set_error(bfd_error_no_error);

    /*The flavour in this function is simply the type of the binary -ELF,PE...-
    if the flavour is unknown it exists with an error message*/
    if(bfd_get_flavor(bfd_h) == bfd_target_unknown_flavour) {
        fprintf(stderr, "unrecognized format for binary '%s' (%s)\n",
        fname.c_str(), bfd_errmsg(bfd_get_error()));
        return NULL;
    }

    return bfd_h;
}