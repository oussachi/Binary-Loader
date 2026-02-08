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


/*Function used to load the symbols, a symbol is represented by the asymbol struct
a symbol table is a asymbol** which is an array of pointers to symbols
This function populates the binary's array of pointers to symbols*/

static int load_symbols_bfd(bfd *bfd_h, Binary *bin) {
    int ret;
    long n, nsyms, i;
    asymbol **bfd_symtab;
    Symbol *sym;

    bfd_symtab = NULL;
    /*This function returns the necessary number of bytes to store all the symbol pointers
    a neg value is an error, and 0 means the binary is stripped -no symbols-*/
    n = bfd_get_symtab_upper_bound(bfd_h);
    if(n < 0) {
        fprintf(stderr, "failed to read symtab (%s)\n",
        bfd_errmsg(bfd_get_error()));
        goto fail;
    } else if(n) {
        bfd_symtab = (asymbol**)malloc(n);
        if(!bfd_symtab) {
            fprintf(stderr, "out of memory\n");
            goto fail;
        }

        /*This function is the most important here as it populates the array bfd_symtab
        with the bfd's symbols and returns how many symbols it populated*/
        nsyms = bfd_canonicalize_symtab(bfd_h, bfd_symtab);
        if(nsyms < 0) {
            fprintf(stderr, "failed to read symtab (%s)\n",
            bfd_errmsg(bfd)bfd_get_error());
            goto fail;
        }
        /*Using the populated array bfd_symtab, we now create Symbol entries in our binary
        and fill it with the data of the array's entries*/
        for(i = 0; i < nsyms; i++) {
            /*As we are only interested in function symbols here, we only check if the function flag is set*/
            if(bfd_symtab[i]->flags & BSF_FUNCTION) {
                bin->symbols.push_back(Symbol());
                sym = &bin->symbols.back();
                sym->type = Symbol::SYM_TYPE_FUNC;
                sym->name = std::string(bfd_symtab[i]->name);
                sym->addr = bfd_asymbol_value(bfd_symtab[i]);
            }
        }
    }

    ret = 0;
    goto cleanup;

    fail:
        ret = -1;

    cleanup:
        if(bfd_symtab) free(bfd_symtab);

    return ret;
}


/*Function used to load the dynamic symbols, practically identical to static symbols
except for functions used*/

static int load_dynsym_bfd(bfd *bfd_h, Binary *bin) {
    int ret;
    long n, nsyms, i;
    asymbol **bfd_dynsym;
    Symbol *sym;

    bfd_dynsym = NULL;
    /*This function returns the necessary number of bytes to store all the symbol pointers
    a neg value is an error, and 0 means the binary is stripped -no symbols-*/
    n = bfd_get_dynamic_symtab_upper_bound(bfd_h);
    if(n < 0) {
        fprintf(stderr, "failed to read dynamic symtab (%s)\n",
        bfd_errmsg(bfd_get_error()));
        goto fail;
    } else if(n) {
        bfd_dynsym = (asymbol**)malloc(n);
        if(!bfd_dynsym) {
            fprintf(stderr, "out of memory\n");
            goto fail;
        }

        /*This function is the most important here as it populates the array bfd_dynsym
        with the bfd's symbols and returns how many symbols it populated*/
        nsyms = bfd_canonicalize_dynamic_symtab(bfd_h, bfd_dynsym);
        if(nsyms < 0) {
            fprintf(stderr, "failed to read dynamic symtab (%s)\n",
            bfd_errmsg(bfd)bfd_get_error());
            goto fail;
        }
        /*Using the populated array bfd_dynsym, we now create Symbol entries in our binary
        and fill it with the data of the array's entries*/
        for(i = 0; i < nsyms; i++) {
            if(bfd_dynsym[i]->flags & BSF_FUNCTION) {
                bin->symbols.push_back(Symbol());
                sym = &bin->symbols.back();
                sym->type = Symbol::SYM_TYPE_FUNC;
                sym->name = std::string(bfd_dynsym[i]->name);
                sym->addr = bfd_asymbol_value(bfd_dynsym[i]);
            }
        }
    }

    ret = 0;
    goto cleanup;

    fail:
        ret = -1;

    cleanup:
        if(bfd_dynsym) free(bfd_dynsym);

    return ret;
}


static int load_binary_bfd(std::string &fname, Binary *bin, Binary::BinaryType type) {
    int ret;
    bfd *bfd_h;
    const bfd_arch_info_type *bfd_info;

    bfd_h = NULL;
    /*Binary is opened using the previously defined open_bfd function*/
    bfd_h = open_bfd(fname);
    if(!bfd_h) {
        goto fail;
    }

    /*Some of the binary's properties are set*/
    bin->filename = std::string(fname);
    bin->entry = bfd_get_start_address(bfd_h);
    bin->type_str = std::string(bfd_h->xvec->name);

    /*The xvec field gives us access to the bfd_target strcuture which includes
    data about the type of the binary*/
    switch(bfd_h->xvec->flavour){
        case bfd_target_elf_flavour:
            bin->type = Binary::BIN_TYPE_ELF;
            break;
        case bfd_target_coff_flavour:
            bin->type = Binary::BIN_TYPE_PE;
            break;
        case bfd_target_unknown_flavour:
        default:
            fprintf(stderr, "unsupported binary type (%s)\n", bfd_h->xvec->name);
            goto fail;
    }

    bfd_info = bfd_get_arch_info(bfd_h);
    bin->arch_str = std::string(bfd_info->printable_name);

    /*We here use the structure returned by bfd_get_arch_info to get data about
    the architecture and then decide the binary's arch*/
    switch(bfd_info->mach):
        case bfd_mach_i386_i386:
            bin->arch = Binary::ARCH_X86;
            bin->bits = 32;
            break;
        case fd_mach_x86_64:
            bin->arch = Binary::ARCH_X86;
            bin->bits = 64;
            break;
        default:
            fprintf(stderr, "unsupported architecture (%s)\n",
                bfd_info->printable_name); 
            goto fail;

    /* Symbol handling is best effort only as they may not be present*/
    load_symbols_bfd(bfd_h, bin);
    load_dynsym_bfd(bfd_h, bin);

    if(load_sections_bfd(bfd-h, bin) < 0) goto fail;

    ret = 0;
    goto cleanup;

    fail:
        ret = -1;

    cleanup:
        if(bfd_h) bfd_close(bfd_h);

    return ret;
}