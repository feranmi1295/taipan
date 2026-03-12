#define _GNU_SOURCE
#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ─────────────────────────────────────────────
//  Entity registry — maps name → field list
//  Needed so field access can compute GEP index
// ─────────────────────────────────────────────
#define MAX_ENTITIES   64
#define MAX_FIELDS     32

typedef struct {
    char name[64];
    char lltype[64];    // LLVM type string
} EntityField;

typedef struct {
    char name[64];
    char ret_lltype[64];
} EntityMethod;

typedef struct {
    char         name[64];
    EntityField  fields[MAX_FIELDS];
    int          field_count;
    EntityMethod methods[64];
    int          method_count;
} EntityInfo;

static EntityInfo  entity_registry[MAX_ENTITIES];
static int         entity_count = 0;

static EntityInfo *entity_find(const char *name) {
    for (int i = 0; i < entity_count; i++)
        if (!strcmp(entity_registry[i].name, name)) return &entity_registry[i];
    return NULL;
}

static EntityInfo *entity_register(const char *name) {
    if (entity_count >= MAX_ENTITIES) return NULL;
    EntityInfo *e = &entity_registry[entity_count++];
    memset(e, 0, sizeof(EntityInfo));
    strncpy(e->name, name, 63);
    return e;
}

static int entity_field_index(EntityInfo *e, const char *field) {
    for (int i = 0; i < e->field_count; i++)
        if (!strcmp(e->fields[i].name, field)) return i;
    return -1;
}

// After we know an entity name, lltype_of_node should return %EntityName*
// We check the registry to decide
static void lltype_of_name(const char *name, char *buf, int bufsz) {
    if (entity_find(name)) {
        snprintf(buf, bufsz, "%%struct.%s*", name);
        return;
    }
    // fallback for primitive type names
    if      (!strcmp(name,"i8"))     strncpy(buf,"i8",    bufsz);
    else if (!strcmp(name,"i16"))    strncpy(buf,"i16",   bufsz);
    else if (!strcmp(name,"i32"))    strncpy(buf,"i32",   bufsz);
    else if (!strcmp(name,"i64"))    strncpy(buf,"i64",   bufsz);
    else if (!strcmp(name,"u8"))     strncpy(buf,"i8",    bufsz);
    else if (!strcmp(name,"u16"))    strncpy(buf,"i16",   bufsz);
    else if (!strcmp(name,"u32"))    strncpy(buf,"i32",   bufsz);
    else if (!strcmp(name,"u64"))    strncpy(buf,"i64",   bufsz);
    else if (!strcmp(name,"f32"))    strncpy(buf,"float", bufsz);
    else if (!strcmp(name,"f64"))    strncpy(buf,"double",bufsz);
    else if (!strcmp(name,"bool"))   strncpy(buf,"i1",    bufsz);
    else if (!strcmp(name,"char"))   strncpy(buf,"i8",    bufsz);
    else if (!strcmp(name,"str"))    strncpy(buf,"i8*",   bufsz);
    else                             snprintf(buf,bufsz,"%%struct.%s*",name);
}



// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────

static int  next_reg  (Codegen *cg) { return cg->reg_counter++;   }
static int  next_label(Codegen *cg) { return cg->label_counter++; }

static void emit(Codegen *cg, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(cg->out, fmt, ap);
    va_end(ap);
}

static void cg_error(Codegen *cg, const ASTNode *n, const char *msg) {
    cg->had_error = 1;
    fprintf(stderr, "[line %d:%d] CodegenError: %s\n",
            n ? n->line : 0, n ? n->col : 0, msg);
}

// ─────────────────────────────────────────────
//  Type → LLVM IR type string
// ─────────────────────────────────────────────
static void lltype_of_node(ASTNode *type_node, char *buf, int bufsz) {
    if (!type_node) { strncpy(buf, "void", bufsz); return; }

    if (type_node->type == NODE_POINTER_TYPE) {
        char base[32];
        lltype_of_node(type_node->as.pointer_type.base, base, sizeof(base));
        snprintf(buf, bufsz, "%s*", base);
        return;
    }
    if (type_node->type == NODE_ARRAY_TYPE) {
        char elem[32];
        lltype_of_node(type_node->as.array_type.elem_type, elem, sizeof(elem));
        int sz = type_node->as.array_type.size;
        if (sz > 0) snprintf(buf, bufsz, "[%d x %s]", sz, elem);
        else        snprintf(buf, bufsz, "%s*", elem);  // dynamic → pointer
        return;
    }
    // NODE_TYPE
    const char *name = type_node->as.type_node.name;
    if      (!strcmp(name,"i8"))     strncpy(buf, "i8",     bufsz);
    else if (!strcmp(name,"i16"))    strncpy(buf, "i16",    bufsz);
    else if (!strcmp(name,"i32"))    strncpy(buf, "i32",    bufsz);
    else if (!strcmp(name,"i64"))    strncpy(buf, "i64",    bufsz);
    else if (!strcmp(name,"u8"))     strncpy(buf, "i8",     bufsz);
    else if (!strcmp(name,"u16"))    strncpy(buf, "i16",    bufsz);
    else if (!strcmp(name,"u32"))    strncpy(buf, "i32",    bufsz);
    else if (!strcmp(name,"u64"))    strncpy(buf, "i64",    bufsz);
    else if (!strcmp(name,"f32"))    strncpy(buf, "float",  bufsz);
    else if (!strcmp(name,"f64"))    strncpy(buf, "double", bufsz);
    else if (!strcmp(name,"bool"))   strncpy(buf, "i1",     bufsz);
    else if (!strcmp(name,"char"))   strncpy(buf, "i8",     bufsz);
    else if (!strcmp(name,"str"))    strncpy(buf, "i8*",    bufsz);
    else if (!strcmp(name,"tensor")) strncpy(buf, "i8*",    bufsz);
    else if (!strcmp(name,"any"))    strncpy(buf, "i8*",    bufsz);
    else {
        // Could be an entity name
        if (entity_find(name)) snprintf(buf, bufsz, "%%struct.%s*", name);
        else                    strncpy(buf, "i8*", bufsz);
    }
}

static int is_float_type(const char *lltype) {
    return strcmp(lltype,"float")==0 || strcmp(lltype,"double")==0;
}


// ─────────────────────────────────────────────
//  Scope management
// ─────────────────────────────────────────────
static void cg_scope_push(Codegen *cg) {
    CGScope *s  = calloc(1, sizeof(CGScope));
    s->parent   = cg->scope;
    cg->scope   = s;
}

static void cg_scope_pop(Codegen *cg) {
    CGScope *old = cg->scope;
    cg->scope    = old->parent;
    CGSymbol *sym = old->symbols;
    while (sym) { CGSymbol *nx = sym->next; free(sym); sym = nx; }
    free(old);
}

static void cg_scope_define(Codegen *cg, const char *name,
                             const char *lltype, const char *alloca_reg) {
    CGSymbol *sym = calloc(1, sizeof(CGSymbol));
    strncpy(sym->name,       name,       sizeof(sym->name)-1);
    strncpy(sym->lltype,     lltype,     sizeof(sym->lltype)-1);
    strncpy(sym->alloca_reg, alloca_reg, sizeof(sym->alloca_reg)-1);
    sym->next         = cg->scope->symbols;
    cg->scope->symbols = sym;
}

static CGSymbol *cg_scope_lookup(Codegen *cg, const char *name) {
    for (CGScope *s = cg->scope; s; s = s->parent)
        for (CGSymbol *sym = s->symbols; sym; sym = sym->next)
            if (!strcmp(sym->name, name)) return sym;
    return NULL;
}

// ─────────────────────────────────────────────
//  String literal interning
// ─────────────────────────────────────────────
static int intern_string(Codegen *cg, const char *str) {
    for (int i = 0; i < cg->str_count; i++)
        if (!strcmp(cg->str_literals[i], str)) return cg->str_ids[i];
    int id = cg->str_count;
    cg->str_literals = realloc(cg->str_literals, sizeof(char*) * (id+1));
    cg->str_ids      = realloc(cg->str_ids,      sizeof(int)   * (id+1));
    cg->str_literals[id] = strdup(str);
    cg->str_ids[id]      = id;
    cg->str_count++;
    return id;
}

// ─────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────
static CGValue cg_expr (Codegen *cg, ASTNode *node);
static void    cg_stmt (Codegen *cg, ASTNode *node);
static void    cg_block(Codegen *cg, ASTNode *node);

// ─────────────────────────────────────────────
//  Expression codegen → returns SSA value
// ─────────────────────────────────────────────
static CGValue cg_expr(Codegen *cg, ASTNode *node) {
    CGValue val;
    memset(&val, 0, sizeof(val));

    if (!node) {
        snprintf(val.name, sizeof(val.name), "0");
        snprintf(val.lltype, sizeof(val.lltype), "i32");
        return val;
    }

    switch (node->type) {

        // ── Literals ─────────────────────────
        case NODE_INT_LIT:
            snprintf(val.name,   sizeof(val.name),   "%lld", node->as.int_lit.value);
            snprintf(val.lltype, sizeof(val.lltype),  "i32");
            return val;

        case NODE_FLOAT_LIT:
            {
                // Round to float precision first, then get double bits of that value
                // LLVM IR: float constants can be expressed as double hex 0xHHHH...
                // where the hex represents the double bits of the float-rounded value
                float  _fv = (float)node->as.float_lit.value;
                double _dv = (double)_fv;  // exact float value as double
                unsigned long long _bits;
                memcpy(&_bits, &_dv, sizeof(_dv));
                snprintf(val.name, sizeof(val.name), "0x%016llX", (unsigned long long)_bits);
            }
            snprintf(val.lltype, sizeof(val.lltype),  "float");
            return val;

        case NODE_BOOL_LIT:
            snprintf(val.name,   sizeof(val.name),   "%d", node->as.bool_lit.value);
            snprintf(val.lltype, sizeof(val.lltype),  "i1");
            return val;

        case NODE_CHAR_LIT:
            snprintf(val.name,   sizeof(val.name),   "%d", (int)node->as.char_lit.value);
            snprintf(val.lltype, sizeof(val.lltype),  "i8");
            return val;

        case NODE_STRING_LIT: {
            int sid  = intern_string(cg, node->as.string_lit.value);
            int len  = (int)strlen(node->as.string_lit.value) + 1;
            int reg  = next_reg(cg);
            emit(cg, "  %%%d = getelementptr inbounds [%d x i8], [%d x i8]* @.str.%d, i32 0, i32 0\n",
                 reg, len, len, sid);
            snprintf(val.name,   sizeof(val.name),   "%%%d", reg);
            snprintf(val.lltype, sizeof(val.lltype),  "i8*");
            return val;
        }

        // ── Array literal ────────────────────
        case NODE_ARRAY_LIT: {
            // Heap-allocate via __taipan_array_new so array_len works
            int count = node->as.array_lit.count;
            if (count == 0) {
                snprintf(val.name,   sizeof(val.name),   "null");
                snprintf(val.lltype, sizeof(val.lltype),  "i8*");
                return val;
            }
            CGValue first = cg_expr(cg, node->as.array_lit.elements[0]);
            // Compute elem size via sizeof trick
            int sz_reg = next_reg(cg);
            emit(cg, "  %%%d = getelementptr inbounds %s, %s* null, i32 1\n", sz_reg, first.lltype, first.lltype);
            int szi_reg = next_reg(cg);
            emit(cg, "  %%%d = ptrtoint %s* %%%d to i32\n", szi_reg, first.lltype, sz_reg);
            // Call __taipan_array_new(count, elem_size) -> i8*
            int raw_reg = next_reg(cg);
            emit(cg, "  %%%d = call i8* @__taipan_array_new(i32 %d, i32 %%%d)\n", raw_reg, count, szi_reg);
            // Bitcast to elem*
            int arr_reg = next_reg(cg);
            emit(cg, "  %%%d = bitcast i8* %%%d to %s*\n", arr_reg, raw_reg, first.lltype);
            // Store each element
            for (int i = 0; i < count; i++) {
                CGValue ev = (i == 0) ? first : cg_expr(cg, node->as.array_lit.elements[i]);
                int ptr_reg = next_reg(cg);
                emit(cg, "  %%%d = getelementptr inbounds %s, %s* %%%d, i32 %d\n",
                     ptr_reg, first.lltype, first.lltype, arr_reg, i);
                emit(cg, "  store %s %s, %s* %%%d\n", ev.lltype, ev.name, ev.lltype, ptr_reg);
            }
            snprintf(val.name,   sizeof(val.name),   "%%%d", arr_reg);
            snprintf(val.lltype, sizeof(val.lltype),  "%s*", first.lltype);
            return val;
        }

        // ── Identifier (load from alloca) ────
        case NODE_IDENT: {
            CGSymbol *sym = cg_scope_lookup(cg, node->as.ident.name);
            if (!sym) {
                // Could be a function name — return as global ref
                snprintf(val.name,   sizeof(val.name),   "@%s", node->as.ident.name);
                snprintf(val.lltype, sizeof(val.lltype),  "i8*");
                return val;
            }
            // Entity field shortcut: alloca_reg = "self:FIELD:IDX:STYPE"
            if (strncmp(sym->alloca_reg, "self:", 5) == 0) {
                // Parse: self:fieldname:index:structtype
                char tmp[256]; strncpy(tmp, sym->alloca_reg + 5, sizeof(tmp)-1);
                char *colon1 = strchr(tmp, ':');
                char *colon2 = colon1 ? strchr(colon1+1, ':') : NULL;
                int field_idx = colon1 ? atoi(colon1+1) : 0;
                char stype[64] = "%struct.Unknown";
                if (colon2) strncpy(stype, colon2+1, sizeof(stype)-1);

                // Load self pointer first
                CGSymbol *self_sym = cg_scope_lookup(cg, "self");
                int self_reg = next_reg(cg);
                emit(cg, "  %%%d = load %s, %s* %s\n",
                     self_reg, self_sym->lltype, self_sym->lltype, self_sym->alloca_reg);

                // GEP to field
                int gep = next_reg(cg);
                emit(cg, "  %%%d = getelementptr inbounds %s, %s %%%d, i32 0, i32 %d\n",
                     gep, stype, self_sym->lltype, self_reg, field_idx);

                // Load field value
                int load = next_reg(cg);
                emit(cg, "  %%%d = load %s, %s* %%%d\n", load, sym->lltype, sym->lltype, gep);

                snprintf(val.name,   sizeof(val.name),   "%%%d", load);
                snprintf(val.lltype, sizeof(val.lltype),  "%s", sym->lltype);
                return val;
            }
            int reg = next_reg(cg);
            emit(cg, "  %%%d = load %s, %s* %s\n",
                 reg, sym->lltype, sym->lltype, sym->alloca_reg);
            snprintf(val.name,   sizeof(val.name),   "%%%d", reg);
            snprintf(val.lltype, sizeof(val.lltype),  "%s", sym->lltype);
            return val;
        }

        // ── Unary ────────────────────────────
        case NODE_UNARY: {
            CGValue operand = cg_expr(cg, node->as.unary.operand);
            int reg = next_reg(cg);
            if (!strcmp(node->as.unary.op, "-")) {
                if (is_float_type(operand.lltype))
                    emit(cg, "  %%%d = fneg %s %s\n", reg, operand.lltype, operand.name);
                else
                    emit(cg, "  %%%d = sub %s 0, %s\n", reg, operand.lltype, operand.name);
            } else { // !
                emit(cg, "  %%%d = xor i1 %s, true\n", reg, operand.name);
            }
            snprintf(val.name,   sizeof(val.name),   "%%%d", reg);
            snprintf(val.lltype, sizeof(val.lltype),  "%s", operand.lltype);
            return val;
        }

        // ── Binary ───────────────────────────
        case NODE_BINARY: {
            CGValue left  = cg_expr(cg, node->as.binary.left);
            CGValue right = cg_expr(cg, node->as.binary.right);
            int reg = next_reg(cg);
            const char *op = node->as.binary.op;
            int is_fp = is_float_type(left.lltype);

            if      (!strcmp(op,"+"))  emit(cg, "  %%%d = %s %s %s, %s\n", reg, is_fp?"fadd":"add",  left.lltype, left.name, right.name);
            else if (!strcmp(op,"-"))  emit(cg, "  %%%d = %s %s %s, %s\n", reg, is_fp?"fsub":"sub",  left.lltype, left.name, right.name);
            else if (!strcmp(op,"*"))  emit(cg, "  %%%d = %s %s %s, %s\n", reg, is_fp?"fmul":"mul",  left.lltype, left.name, right.name);
            else if (!strcmp(op,"/"))  emit(cg, "  %%%d = %s %s %s, %s\n", reg, is_fp?"fdiv":"sdiv", left.lltype, left.name, right.name);
            else if (!strcmp(op,"%"))  emit(cg, "  %%%d = srem %s %s, %s\n", reg,                    left.lltype, left.name, right.name);
            else if (!strcmp(op,"==")) emit(cg, "  %%%d = %s eq  %s %s, %s\n", reg, is_fp?"fcmp":"icmp", left.lltype, left.name, right.name);
            else if (!strcmp(op,"!=")) emit(cg, "  %%%d = %s ne  %s %s, %s\n", reg, is_fp?"fcmp":"icmp", left.lltype, left.name, right.name);
            else if (!strcmp(op,"<"))  emit(cg, "  %%%d = %s %s  %s %s, %s\n", reg, is_fp?"fcmp":"icmp", is_fp?"olt":"slt", left.lltype, left.name, right.name);
            else if (!strcmp(op,">"))  emit(cg, "  %%%d = %s %s  %s %s, %s\n", reg, is_fp?"fcmp":"icmp", is_fp?"ogt":"sgt", left.lltype, left.name, right.name);
            else if (!strcmp(op,"<=")) emit(cg, "  %%%d = %s %s  %s %s, %s\n", reg, is_fp?"fcmp":"icmp", is_fp?"ole":"sle", left.lltype, left.name, right.name);
            else if (!strcmp(op,">=")) emit(cg, "  %%%d = %s %s  %s %s, %s\n", reg, is_fp?"fcmp":"icmp", is_fp?"oge":"sge", left.lltype, left.name, right.name);
            else if (!strcmp(op,"&"))  emit(cg, "  %%%d = and i1 %s, %s\n", reg, left.name, right.name);
            else if (!strcmp(op,"|"))  emit(cg, "  %%%d = or  i1 %s, %s\n", reg, left.name, right.name);

            // Result type: comparisons → i1, others → left type
            const char *rtype = (!strcmp(op,"==")||!strcmp(op,"!=")||
                                 !strcmp(op,"<") ||!strcmp(op,">")||
                                 !strcmp(op,"<=")||!strcmp(op,">=")||
                                 !strcmp(op,"&") ||!strcmp(op,"|")) ? "i1" : left.lltype;
            snprintf(val.name,   sizeof(val.name),   "%%%d", reg);
            snprintf(val.lltype, sizeof(val.lltype),  "%s", rtype);
            return val;
        }

        // ── Assign (=, +=, -=, *=, /=) ──────
        case NODE_ASSIGN: {
            CGValue rhs = cg_expr(cg, node->as.assign.value);
            const char *op = node->as.assign.op;

            // Resolve target alloca
            ASTNode *tgt_node = node->as.assign.target;
            char alloca_reg[64] = {0};
            char ltype[64]      = {0};

            if (tgt_node->type == NODE_IDENT) {
                CGSymbol *sym = cg_scope_lookup(cg, tgt_node->as.ident.name);
                if (sym && strncmp(sym->alloca_reg, "self:", 5) == 0) {
                    // Entity field write-back: GEP from self then store
                    char tmp2[256]; strncpy(tmp2, sym->alloca_reg+5, 255);
                    char *fc1 = strchr(tmp2, ':');
                    if (!fc1) { break; }
                    int fidx = atoi(fc1+1);
                    char *fc2 = strchr(fc1+1, ':');
                    if (!fc2) { break; }
                    char fstype[64]; strncpy(fstype, fc2+1, 63);
                    // strip trailing * if present
                    int fstlen = (int)strlen(fstype);
                    if (fstlen > 0 && fstype[fstlen-1] == '*') fstype[fstlen-1] = '\0';
                    char fsptr[96]; snprintf(fsptr, 96, "%s*", fstype);
                    CGSymbol *ss2 = cg_scope_lookup(cg, "self");
                    if (!ss2) { break; }
                    int sl = next_reg(cg);
                    emit(cg, "  %%%d = load %s, %s* %s\n", sl, fsptr, fsptr, ss2->alloca_reg);
                    int fg = next_reg(cg);
                    emit(cg, "  %%%d = getelementptr inbounds %s, %s %%%d, i32 0, i32 %d\n",
                         fg, fstype, fsptr, sl, fidx);
                    // Compound op support
                    CGValue store_val = rhs;
                    if (strcmp(op, "=") != 0) {
                        int fl = next_reg(cg);
                        emit(cg, "  %%%d = load %s, %s* %%%d\n", fl, sym->lltype, sym->lltype, fg);
                        int fo = next_reg(cg);
                        int fis_fp = is_float_type(sym->lltype);
                        if      (!strcmp(op,"+=")) emit(cg,"  %%%d = %s %s %%%d, %s\n",fo,fis_fp?"fadd":"add", sym->lltype,fl,rhs.name);
                        else if (!strcmp(op,"-=")) emit(cg,"  %%%d = %s %s %%%d, %s\n",fo,fis_fp?"fsub":"sub", sym->lltype,fl,rhs.name);
                        else if (!strcmp(op,"*=")) emit(cg,"  %%%d = %s %s %%%d, %s\n",fo,fis_fp?"fmul":"mul", sym->lltype,fl,rhs.name);
                        else if (!strcmp(op,"/=")) emit(cg,"  %%%d = %s %s %%%d, %s\n",fo,fis_fp?"fdiv":"sdiv",sym->lltype,fl,rhs.name);
                        snprintf(store_val.name,   sizeof(store_val.name),   "%%%d", fo);
                        snprintf(store_val.lltype, sizeof(store_val.lltype),  "%s",  sym->lltype);
                    }
                    emit(cg, "  store %s %s, %s* %%%d\n", store_val.lltype, store_val.name, store_val.lltype, fg);
                    return store_val;
                } else if (sym) {
                    strncpy(alloca_reg, sym->alloca_reg, sizeof(alloca_reg)-1);
                    strncpy(ltype,      sym->lltype,     sizeof(ltype)-1);
                }
            } else if (tgt_node->type == NODE_INDEX) {
                // ptr[i] = val
                CGValue ptr = cg_expr(cg, tgt_node->as.index_expr.target);
                CGValue idx = cg_expr(cg, tgt_node->as.index_expr.index);
                int gep = next_reg(cg);
                // strip trailing * from ptr type to get base
                char base_type[64];
                strncpy(base_type, ptr.lltype, sizeof(base_type)-1);
                int blen = (int)strlen(base_type);
                if (blen > 0 && base_type[blen-1] == '*') base_type[blen-1] = '\0';
                emit(cg, "  %%%d = getelementptr inbounds %s, %s %s, i32 %s\n",
                     gep, base_type, ptr.lltype, ptr.name, idx.name);
                emit(cg, "  store %s %s, %s* %%%d\n",
                     rhs.lltype, rhs.name, rhs.lltype, gep);
                return rhs;
            }

            // Compound assignment: load, operate, store
            if (strcmp(op, "=") != 0 && alloca_reg[0]) {
                int load_reg = next_reg(cg);
                emit(cg, "  %%%d = load %s, %s* %s\n",
                     load_reg, ltype, ltype, alloca_reg);
                int op_reg = next_reg(cg);
                int is_fp = is_float_type(ltype);
                if      (!strcmp(op,"+=")) emit(cg, "  %%%d = %s %s %%%d, %s\n", op_reg, is_fp?"fadd":"add",  ltype, load_reg, rhs.name);
                else if (!strcmp(op,"-=")) emit(cg, "  %%%d = %s %s %%%d, %s\n", op_reg, is_fp?"fsub":"sub",  ltype, load_reg, rhs.name);
                else if (!strcmp(op,"*=")) emit(cg, "  %%%d = %s %s %%%d, %s\n", op_reg, is_fp?"fmul":"mul",  ltype, load_reg, rhs.name);
                else if (!strcmp(op,"/=")) emit(cg, "  %%%d = %s %s %%%d, %s\n", op_reg, is_fp?"fdiv":"sdiv", ltype, load_reg, rhs.name);
                emit(cg, "  store %s %%%d, %s* %s\n", ltype, op_reg, ltype, alloca_reg);
                snprintf(val.name,   sizeof(val.name),   "%%%d", op_reg);
                snprintf(val.lltype, sizeof(val.lltype),  "%s", ltype);
                return val;
            }

            // Simple store
            if (alloca_reg[0])
                emit(cg, "  store %s %s, %s* %s\n", rhs.lltype, rhs.name, rhs.lltype, alloca_reg);
            return rhs;
        }

        // ── Index access: arr[i] ─────────────
        case NODE_INDEX: {
            CGValue ptr = cg_expr(cg, node->as.index_expr.target);
            CGValue idx = cg_expr(cg, node->as.index_expr.index);
            // Get base type (strip *)
            char base_type[64];
            strncpy(base_type, ptr.lltype, sizeof(base_type)-1);
            int blen = (int)strlen(base_type);
            if (blen > 0 && base_type[blen-1] == '*') base_type[blen-1] = '\0';
            int gep = next_reg(cg);
            emit(cg, "  %%%d = getelementptr inbounds %s, %s %s, i32 %s\n",
                 gep, base_type, ptr.lltype, ptr.name, idx.name);
            int load = next_reg(cg);
            emit(cg, "  %%%d = load %s, %s* %%%d\n", load, base_type, base_type, gep);
            snprintf(val.name,   sizeof(val.name),   "%%%d", load);
            snprintf(val.lltype, sizeof(val.lltype),  "%s", base_type);
            return val;
        }

        // ── Member access: obj.field ─────────
        case NODE_MEMBER: {
            CGValue obj = cg_expr(cg, node->as.member.object);
            const char *field = node->as.member.field;

            // Determine entity type from pointer type string
            // obj.lltype is like "%struct.Point*"
            char ent_name[64] = {0};
            if (strncmp(obj.lltype, "%struct.", 8) == 0) {
                strncpy(ent_name, obj.lltype + 8, sizeof(ent_name)-1);
                // strip trailing *
                int nl = (int)strlen(ent_name);
                if (nl > 0 && ent_name[nl-1] == '*') ent_name[nl-1] = '\0';
            }

            EntityInfo *ent = entity_find(ent_name);
            if (!ent) {
                // fallback: just return obj
                snprintf(val.name,   sizeof(val.name),   "%s", obj.name);
                snprintf(val.lltype, sizeof(val.lltype),  "i8*");
                return val;
            }

            int idx = entity_field_index(ent, field);
            if (idx < 0) {
                fprintf(stderr, "CodegenError: entity '%s' has no field '%s'\n", ent_name, field);
                snprintf(val.name, sizeof(val.name), "0");
                snprintf(val.lltype, sizeof(val.lltype), "i32");
                return val;
            }

            // GEP to get pointer to field
            char struct_type[96];
            snprintf(struct_type, sizeof(struct_type), "%%struct.%s", ent_name);
            char field_lltype[64];
            strncpy(field_lltype, ent->fields[idx].lltype, sizeof(field_lltype)-1);

            int gep = next_reg(cg);
            emit(cg, "  %%%d = getelementptr inbounds %s, %s* %s, i32 0, i32 %d\n",
                 gep, struct_type, struct_type, obj.name, idx);

            // Load the field value
            int load = next_reg(cg);
            emit(cg, "  %%%d = load %s, %s* %%%d\n", load, field_lltype, field_lltype, gep);

            snprintf(val.name,   sizeof(val.name),   "%%%d", load);
            snprintf(val.lltype, sizeof(val.lltype),  "%s", field_lltype);
            return val;
        }

        // ── Function call ────────────────────
        case NODE_CALL: {
            // Collect arg values
            int argc = node->as.call.arg_count;
            CGValue *args = calloc(argc, sizeof(CGValue));
            for (int i = 0; i < argc; i++)
                args[i] = cg_expr(cg, node->as.call.args[i]);

            // Resolve callee name
            char callee[128] = {0};
            char ret_type[64] = "i32";

            if (node->as.call.callee->type == NODE_IDENT) {
                const char *fname = node->as.call.callee->as.ident.name;

                // Built-ins
                if (!strcmp(fname, "println") || !strcmp(fname, "print")) {
                    int is_println = !strcmp(fname, "println");
                    if (argc > 0) {
                        if (!strcmp(args[0].lltype, "i8*")) {
                            if (is_println) {
                                int pr=next_reg(cg);
                                emit(cg, "  %%%d = call i32 @puts(i8* %s)\n", pr, args[0].name);
                                (void)pr;
                            } else {
                                // print: use printf with %s, no newline
                                int fr=next_reg(cg);
                                emit(cg, "  %%%d = getelementptr inbounds [3 x i8], [3 x i8]* @.fmts, i32 0, i32 0\n", fr);
                                int pr=next_reg(cg);
                                emit(cg, "  %%%d = call i32 (i8*, ...) @printf(i8* %%%d, i8* %s)\n", pr, fr, args[0].name);
                                (void)pr;
                            }
                        } else if (!strcmp(args[0].lltype, "float")) {
                            const char *fmt_sym = is_println ? "@.fmtf" : "@.fmtf_no_nl";
                            const char *fmt_sz  = is_println ? "4" : "3";
                            int fmt_reg = next_reg(cg);
                            emit(cg, "  %%%d = getelementptr inbounds [%s x i8], [%s x i8]* %s, i32 0, i32 0\n", fmt_reg, fmt_sz, fmt_sz, fmt_sym);
                            int dbl = next_reg(cg);
                            emit(cg, "  %%%d = fpext float %s to double\n", dbl, args[0].name);
                            int pr2 = next_reg(cg);
                            emit(cg, "  %%%d = call i32 (i8*, ...) @printf(i8* %%%d, double %%%d)\n", pr2, fmt_reg, dbl);
                            (void)pr2;
                        } else {
                            const char *fmt_sym = is_println ? "@.fmtd" : "@.fmtd_no_nl";
                            const char *fmt_sz  = is_println ? "4" : "3";
                            int fmt_reg = next_reg(cg);
                            emit(cg, "  %%%d = getelementptr inbounds [%s x i8], [%s x i8]* %s, i32 0, i32 0\n", fmt_reg, fmt_sz, fmt_sz, fmt_sym);
                            int pr2 = next_reg(cg);
                            emit(cg, "  %%%d = call i32 (i8*, ...) @printf(i8* %%%d, %s %s)\n", pr2, fmt_reg, args[0].lltype, args[0].name);
                            (void)pr2;
                        }
                        free(args);
                    }
                    snprintf(val.name,   sizeof(val.name),   "0");
                    snprintf(val.lltype, sizeof(val.lltype),  "void");
                    return val;
                }
                if (!strcmp(fname, "alloc")) {
                    // malloc(size * sizeof element) — use i8* malloc
                    int reg = next_reg(cg);
                    emit(cg, "  %%%d = call i8* @malloc(i32 %s)\n", reg, args[0].name);
                    free(args);
                    snprintf(val.name,   sizeof(val.name),   "%%%d", reg);
                    snprintf(val.lltype, sizeof(val.lltype),  "i8*");
                    return val;
                }
                if (!strcmp(fname, "free")) {
                    emit(cg, "  call void @free(i8* %s)\n", args[0].name);
                    free(args);
                    snprintf(val.name,   sizeof(val.name),   "0");
                    snprintf(val.lltype, sizeof(val.lltype),  "void");
                    return val;
                }

                // Check if this is an entity constructor call
                if (entity_find(fname)) {
                    // Entity constructor: Point(x=v, y=v) → @Point__new(v, v)
                    // Args may be named (we pass them positionally in declaration order)
                    EntityInfo *ctor_ent = entity_find(fname);
                    char ctor_ret[96];
                    snprintf(ctor_ret, sizeof(ctor_ret), "%%struct.%s*", fname);
                    int ctor_reg = next_reg(cg);
                    fprintf(cg->out, "  %%%d = call %%struct.%s* @%s__new(", ctor_reg, fname, fname);
                    // emit args in field order
                    int n_fields = ctor_ent->field_count;
                    int n_args   = argc;
                    int n_emit   = n_fields < n_args ? n_fields : n_args;
                    for (int ci = 0; ci < n_emit; ci++) {
                        if (ci) fprintf(cg->out, ", ");
                        fprintf(cg->out, "%s %s", args[ci].lltype, args[ci].name);
                    }
                    fprintf(cg->out, ")\n");
                    free(args);
                    snprintf(val.name,   sizeof(val.name),   "%%%d", ctor_reg);
                    snprintf(val.lltype, sizeof(val.lltype),  "%%struct.%s*", fname);
                    return val;
                }
                // Check if this is an intra-method call (bare method name inside entity)
                if (cg->current_entity[0]) {
                    EntityInfo *ecur = entity_find(cg->current_entity);
                    int is_method = 0;
                    if (ecur) {
                        for (int mi3 = 0; mi3 < ecur->method_count; mi3++) {
                            if (!strcmp(ecur->methods[mi3].name, fname)) {
                                is_method = 1;
                                strncpy(ret_type, ecur->methods[mi3].ret_lltype, sizeof(ret_type)-1);
                                break;
                            }
                        }
                    }
                    if (is_method) {
                        snprintf(callee, sizeof(callee), "@%s__%s", cg->current_entity, fname);
                        // Prepend self
                        CGSymbol *ss = cg_scope_lookup(cg, "self");
                        if (ss) {
                            int sl = next_reg(cg);
                            char stype[96]; snprintf(stype, sizeof(stype), "%%struct.%s*", cg->current_entity);
                            emit(cg, "  %%%d = load %s, %s* %s\n", sl, stype, stype, ss->alloca_reg);
                            CGValue *new_args2 = calloc(argc + 1, sizeof(CGValue));
                            snprintf(new_args2[0].name,   sizeof(new_args2[0].name),   "%%%d", sl);
                            snprintf(new_args2[0].lltype, sizeof(new_args2[0].lltype),  "%s", stype);
                            for (int i = 0; i < argc; i++) new_args2[i+1] = args[i];
                            free(args); args = new_args2; argc++;
                        }
                        goto emit_call;
                    }
                }
                // Type casts: int(x) and float(x)
                if (!strcmp(fname, "int") && argc == 1) {
                    CGValue av = args[0];
                    free(args);
                    if (!strcmp(av.lltype, "float") || !strcmp(av.lltype, "double")) {
                        int r = next_reg(cg);
                        emit(cg, "  %%%d = fptosi %s %s to i32\n", r, av.lltype, av.name);
                        snprintf(val.name,   sizeof(val.name),   "%%%d", r);
                        snprintf(val.lltype, sizeof(val.lltype),  "i32");
                    } else {
                        // already int, return as-is
                        val = av;
                    }
                    return val;
                }
                if (!strcmp(fname, "float") && argc == 1) {
                    CGValue av = args[0];
                    free(args);
                    if (!strcmp(av.lltype, "i32") || !strcmp(av.lltype, "i64")) {
                        int r = next_reg(cg);
                        emit(cg, "  %%%d = sitofp %s %s to float\n", r, av.lltype, av.name);
                        snprintf(val.name,   sizeof(val.name),   "%%%d", r);
                        snprintf(val.lltype, sizeof(val.lltype),  "float");
                    } else {
                        val = av;
                    }
                    return val;
                }
                // std.math builtins
                // Builtin registry — add new stdlib functions here
                static const struct { const char *tp; const char *ll; const char *rt; } g_builtins[] = {
                    // std.math
                    {"sqrt",   "@__taipan_sqrt",   "float"},
                    {"pow",    "@__taipan_pow",    "float"},
                    {"abs_f",  "@__taipan_abs_f",  "float"},
                    {"floor",  "@__taipan_floor",  "float"},
                    {"ceil",   "@__taipan_ceil",   "float"},
                    {"sin",    "@__taipan_sin",    "float"},
                    {"cos",    "@__taipan_cos",    "float"},
                    {"tan",    "@__taipan_tan",    "float"},
                    {"log",    "@__taipan_log",    "float"},
                    {"log2",   "@__taipan_log2",   "float"},
                    {"abs_i",  "@__taipan_abs_i",  "i32"},
                    {"min_i",  "@__taipan_min_i",  "i32"},
                    {"max_i",  "@__taipan_max_i",  "i32"},
                    {"min_f",  "@__taipan_min_f",  "float"},
                    {"max_f",  "@__taipan_max_f",  "float"},
                    {"clamp_f","@__taipan_clamp_f","float"},
                    {"clamp_i","@__taipan_clamp_i","i32"},
                    // std.io
                    {"read_line",  "@__taipan_read_line",  "i8*"},
                    {"read_int",   "@__taipan_read_int",   "i32"},
                    {"read_float", "@__taipan_read_float", "float"},
                    // std.string
                    {"str_upper",    "@__taipan_str_upper",    "i8*"},
                    {"str_lower",    "@__taipan_str_lower",    "i8*"},
                    {"str_trim",     "@__taipan_str_trim",     "i8*"},
                    {"str_contains", "@__taipan_str_contains", "i32"},
                    {"str_replace",  "@__taipan_str_replace",  "i8*"},
                    {"str_split",    "@__taipan_str_split",    "i8*"},
                    {"str_to_int",   "@__taipan_str_to_int",   "i32"},
                    {"str_to_float", "@__taipan_str_to_float", "float"},
                    {"int_to_str",   "@__taipan_int_to_str",   "i8*"},
                    {"float_to_str", "@__taipan_float_to_str", "i8*"},
                    // std.rand
                    {"rand_int",   "@__taipan_rand_int",   "i32"},
                    {"rand_float", "@__taipan_rand_float", "float"},
                    {"rand_seed",  "@__taipan_rand_seed",  "i32"},
                    // std.time
                    {"time_now",       "@__taipan_time_now",       "i64"},
                    {"time_sleep",     "@__taipan_time_sleep",     "i32"},
                    {"time_timestamp", "@__taipan_time_timestamp", "i64"},
                    // std.env
                    {"getenv", "@__taipan_getenv", "i8*"},
                    // std.mem
                    {"mem_copy", "@__taipan_mem_copy", "i8*"},
                    {"mem_zero", "@__taipan_mem_zero", "i8*"},
                    // std.process
                    {"exec", "@__taipan_exec", "i32"},
                    {NULL, NULL, NULL}
                };
                int builtin_matched = 0;
                for (int bi = 0; g_builtins[bi].tp; bi++) {
                    if (!strcmp(fname, g_builtins[bi].tp)) {
                        int r = next_reg(cg);
                        fprintf(cg->out, "  %%%d = call %s %s(", r, g_builtins[bi].rt, g_builtins[bi].ll);
                        for (int ai = 0; ai < argc; ai++) {
                            if (ai) fprintf(cg->out, ", ");
                            fprintf(cg->out, "%s %s", args[ai].lltype, args[ai].name);
                        }
                        fprintf(cg->out, ")\n");
                        free(args);
                        snprintf(val.name,   sizeof(val.name),   "%%%d", r);
                        snprintf(val.lltype, sizeof(val.lltype),  "%s", g_builtins[bi].rt);
                        builtin_matched = 1;
                        return val;
                    }
                }
                if (!builtin_matched)
                snprintf(callee, sizeof(callee), "@%s", fname);
            } else if (node->as.call.callee->type == NODE_MEMBER) {
                // method call: obj.method(args) → @Entity__method(self, args)
                ASTNode *obj_node = node->as.call.callee->as.member.object;
                const char *method_name = node->as.call.callee->as.member.field;

                // Evaluate self (the object)
                CGValue self_val = cg_expr(cg, obj_node);

                // ── str built-in methods ─────────────────
                if (!strcmp(self_val.lltype, "i8*")) {
                    // Load self if it's behind a pointer (alloca of i8*)
                    CGValue str_self = self_val;
                    if (strcmp(self_val.lltype, "i8*") == 0 && self_val.name[0] == '%') {
                        // already i8* value, use directly
                    }
                    if (!strcmp(method_name, "len")) {
                        int r = next_reg(cg);
                        emit(cg, "  %%%d = call i32 @__taipan_str_len(i8* %s)\n", r, str_self.name);
                        free(args);
                        snprintf(val.name,   sizeof(val.name),   "%%%d", r);
                        snprintf(val.lltype, sizeof(val.lltype),  "i32");
                        return val;
                    } else if (!strcmp(method_name, "concat")) {
                        int r = next_reg(cg);
                        const char *arg0 = argc > 0 ? args[0].name : "null";
                        emit(cg, "  %%%d = call i8* @__taipan_str_concat(i8* %s, i8* %s)\n", r, str_self.name, arg0);
                        free(args);
                        snprintf(val.name,   sizeof(val.name),   "%%%d", r);
                        snprintf(val.lltype, sizeof(val.lltype),  "i8*");
                        return val;
                    } else if (!strcmp(method_name, "eq")) {
                        int r = next_reg(cg);
                        const char *arg0 = argc > 0 ? args[0].name : "null";
                        emit(cg, "  %%%d = call i32 @__taipan_str_eq(i8* %s, i8* %s)\n", r, str_self.name, arg0);
                        free(args);
                        snprintf(val.name,   sizeof(val.name),   "%%%d", r);
                        snprintf(val.lltype, sizeof(val.lltype),  "i32");
                        return val;
                    } else if (!strcmp(method_name, "slice")) {
                        int r = next_reg(cg);
                        const char *a0 = argc > 0 ? args[0].name : "0";
                        const char *a1 = argc > 1 ? args[1].name : "0";
                        emit(cg, "  %%%d = call i8* @__taipan_str_slice(i8* %s, i32 %s, i32 %s)\n", r, str_self.name, a0, a1);
                        free(args);
                        snprintf(val.name,   sizeof(val.name),   "%%%d", r);
                        snprintf(val.lltype, sizeof(val.lltype),  "i8*");
                        return val;
                    }
                }
                // Determine entity name from type
                char ent_name2[64] = {0};
                if (strncmp(self_val.lltype, "%struct.", 8) == 0) {
                    strncpy(ent_name2, self_val.lltype + 8, sizeof(ent_name2)-1);
                    int nl2 = (int)strlen(ent_name2);
                    if (nl2 > 0 && ent_name2[nl2-1] == '*') ent_name2[nl2-1] = '\0';
                }

                // Build mangled name: EntityName__methodName
                snprintf(callee, sizeof(callee), "@%s__%s", ent_name2, method_name);

                // Prepend self to args
                CGValue *new_args = calloc(argc + 1, sizeof(CGValue));
                new_args[0] = self_val;
                for (int i = 0; i < argc; i++) new_args[i+1] = args[i];
                free(args);
                args = new_args;
                argc++;

                // Look up actual return type from entity method registry
                int found_ret = 0;
                EntityInfo *ecall = entity_find(ent_name2);
                if (ecall) {
                    for (int mi2 = 0; mi2 < ecall->method_count && !found_ret; mi2++) {
                        if (strcmp(ecall->methods[mi2].name, method_name) == 0) {
                            strncpy(ret_type, ecall->methods[mi2].ret_lltype, sizeof(ret_type)-1);
                            found_ret = 1;
                        }
                    }
                }
                if (!found_ret) strncpy(ret_type, "void", sizeof(ret_type)-1);
            }

            emit_call:;
            // Emit call — void calls must NOT be assigned a register
            if (!strcmp(ret_type, "void")) {
                fprintf(cg->out, "  call void %s(", callee);
                for (int i = 0; i < argc; i++) {
                    if (i) fprintf(cg->out, ", ");
                    fprintf(cg->out, "%s %s", args[i].lltype, args[i].name);
                }
                fprintf(cg->out, ")\n");
                free(args);
                snprintf(val.name,   sizeof(val.name),   "0");
                snprintf(val.lltype, sizeof(val.lltype),  "void");
                return val;
            }
            int reg = next_reg(cg);
            fprintf(cg->out, "  %%%d = call %s %s(", reg, ret_type, callee);
            for (int i = 0; i < argc; i++) {
                if (i) fprintf(cg->out, ", ");
                fprintf(cg->out, "%s %s", args[i].lltype, args[i].name);
            }
            fprintf(cg->out, ")\n");
            free(args);
            snprintf(val.name,   sizeof(val.name),   "%%%d", reg);
            snprintf(val.lltype, sizeof(val.lltype),  "%s", ret_type);
            return val;
        }

        default:
            snprintf(val.name,   sizeof(val.name),   "0");
            snprintf(val.lltype, sizeof(val.lltype),  "i32");
            return val;
    }
}

// ─────────────────────────────────────────────
//  Block codegen
// ─────────────────────────────────────────────
static void cg_block(Codegen *cg, ASTNode *node) {
    if (!node) return;
    cg_scope_push(cg);
    for (int i = 0; i < node->as.block.count; i++)
        cg_stmt(cg, node->as.block.stmts[i]);
    cg_scope_pop(cg);
}

// ─────────────────────────────────────────────
//  Statement codegen
// ─────────────────────────────────────────────
static void cg_stmt(Codegen *cg, ASTNode *node) {
    if (!node) return;

    switch (node->type) {

        case NODE_USE: break; // handled in pass 1

        // ── let x:i32 = expr ─────────────────
        case NODE_LET: {
            char lltype[64] = "i32";
            if (node->as.let.type_node)
                lltype_of_node(node->as.let.type_node, lltype, sizeof(lltype));
            // Evaluate init FIRST to get real type before alloca
            CGValue init_val; memset(&init_val, 0, sizeof(init_val));
            int has_init = 0;
            if (node->as.let.init) {
                init_val = cg_expr(cg, node->as.let.init);
                has_init = 1;
                if (!node->as.let.type_node)
                    strncpy(lltype, init_val.lltype, sizeof(lltype)-1);
            }
            int addr_reg = next_reg(cg);
            emit(cg, "  %%%d = alloca %s\n", addr_reg, lltype);
            if (has_init)
                emit(cg, "  store %s %s, %s* %%%d\n",
                     init_val.lltype, init_val.name, init_val.lltype, addr_reg);
            char alloca_name[64];
            snprintf(alloca_name, sizeof(alloca_name), "%%%d", addr_reg);
            cg_scope_define(cg, node->as.let.name, lltype, alloca_name);
            break;
        }

        // ── return expr ──────────────────────
        case NODE_RETURN: {
            if (node->as.ret.value) {
                CGValue v = cg_expr(cg, node->as.ret.value);
                emit(cg, "  ret %s %s\n", v.lltype, v.name);
            } else {
                emit(cg, "  ret void\n");
            }
            break;
        }

        // ── if / else ────────────────────────
        case NODE_IF: {
            int id      = next_label(cg);
            CGValue cond = cg_expr(cg, node->as.if_stmt.condition);
            // coerce i32 -> i1 if needed
            if (strcmp(cond.lltype, "i1") != 0) {
                int cr = next_reg(cg);
                emit(cg, "  %%%d = icmp ne %s %s, 0\n", cr, cond.lltype, cond.name);
                snprintf(cond.name, sizeof(cond.name), "%%%d", cr);
                snprintf(cond.lltype, sizeof(cond.lltype), "i1");
            }
            emit(cg, "  br i1 %s, label %%if.then.%d, label %%if.else.%d\n",
                 cond.name, id, id);

            emit(cg, "if.then.%d:\n", id);
            cg_block(cg, node->as.if_stmt.then_block);
            emit(cg, "  br label %%if.end.%d\n", id);

            emit(cg, "if.else.%d:\n", id);
            if (node->as.if_stmt.else_block) {
                if (node->as.if_stmt.else_block->type == NODE_IF)
                    cg_stmt(cg, node->as.if_stmt.else_block);
                else
                    cg_block(cg, node->as.if_stmt.else_block);
            }
            emit(cg, "  br label %%if.end.%d\n", id);

            emit(cg, "if.end.%d:\n", id);
            break;
        }

        // ── while ────────────────────────────
        case NODE_WHILE: {
            int id = next_label(cg);
            emit(cg, "  br label %%while.cond.%d\n", id);
            emit(cg, "while.cond.%d:\n", id);
            CGValue cond = cg_expr(cg, node->as.while_stmt.condition);
            emit(cg, "  br i1 %s, label %%while.body.%d, label %%while.end.%d\n",
                 cond.name, id, id);
            emit(cg, "while.body.%d:\n", id);
            cg_block(cg, node->as.while_stmt.body);
            emit(cg, "  br label %%while.cond.%d\n", id);
            emit(cg, "while.end.%d:\n", id);
            break;
        }

        // ── for i in 0..10 ───────────────────
        case NODE_FOR_RANGE: {
            int id = next_label(cg);
            cg_scope_push(cg);

            // Init: let i = start
            CGValue start = cg_expr(cg, node->as.for_range.start);
            int i_addr = next_reg(cg);
            emit(cg, "  %%%d = alloca i32\n", i_addr);
            emit(cg, "  store i32 %s, i32* %%%d\n", start.name, i_addr);
            char i_alloca[32];
            snprintf(i_alloca, sizeof(i_alloca), "%%%d", i_addr);
            cg_scope_define(cg, node->as.for_range.var, "i32", i_alloca);

            emit(cg, "  br label %%for.cond.%d\n", id);
            emit(cg, "for.cond.%d:\n", id);

            // Condition: i < end
            int i_val = next_reg(cg);
            emit(cg, "  %%%d = load i32, i32* %%%d\n", i_val, i_addr);
            CGValue end = cg_expr(cg, node->as.for_range.end);
            int cmp = next_reg(cg);
            emit(cg, "  %%%d = icmp slt i32 %%%d, %s\n", cmp, i_val, end.name);
            emit(cg, "  br i1 %%%d, label %%for.body.%d, label %%for.end.%d\n",
                 cmp, id, id);

            emit(cg, "for.body.%d:\n", id);
            cg_block(cg, node->as.for_range.body);

            // Increment: i += 1
            int i_cur = next_reg(cg);
            emit(cg, "  %%%d = load i32, i32* %%%d\n", i_cur, i_addr);
            int i_inc = next_reg(cg);
            emit(cg, "  %%%d = add i32 %%%d, 1\n", i_inc, i_cur);
            emit(cg, "  store i32 %%%d, i32* %%%d\n", i_inc, i_addr);
            emit(cg, "  br label %%for.cond.%d\n", id);

            emit(cg, "for.end.%d:\n", id);
            cg_scope_pop(cg);
            break;
        }

        // ── for val in arr ───────────────────
        case NODE_FOR_ITER: {
            int id = next_label(cg);
            cg_scope_push(cg);

            // Index counter
            int idx_addr = next_reg(cg);
            emit(cg, "  %%%d = alloca i32\n", idx_addr);
            emit(cg, "  store i32 0, i32* %%%d\n", idx_addr);

            // Get array ptr and length (simplified: use i32 -1 as sentinel → runtime len)
            CGValue arr = cg_expr(cg, node->as.for_iter.iterable);

            // Alloca for loop variable
            char base_type[64];
            strncpy(base_type, arr.lltype, sizeof(base_type)-1);
            int blen = (int)strlen(base_type);
            if (blen > 0 && base_type[blen-1] == '*') base_type[blen-1] = '\0';
            int val_addr = next_reg(cg);
            emit(cg, "  %%%d = alloca %s\n", val_addr, base_type);
            char val_alloca[32];
            snprintf(val_alloca, sizeof(val_alloca), "%%%d", val_addr);
            cg_scope_define(cg, node->as.for_iter.var, base_type, val_alloca);

            // Cast array ptr to i8* for runtime call
            int cast_reg = next_reg(cg);
            emit(cg, "  %%%d = bitcast %s %s to i8*\n", cast_reg, arr.lltype, arr.name);
            int len_reg = next_reg(cg);
            emit(cg, "  %%%d = call i32 @__taipan_array_len(i8* %%%d)\n", len_reg, cast_reg);

            emit(cg, "  br label %%foriter.cond.%d\n", id);
            emit(cg, "foriter.cond.%d:\n", id);
            int i_val = next_reg(cg);
            emit(cg, "  %%%d = load i32, i32* %%%d\n", i_val, idx_addr);
            int cmp = next_reg(cg);
            emit(cg, "  %%%d = icmp slt i32 %%%d, %%%d\n", cmp, i_val, len_reg);
            emit(cg, "  br i1 %%%d, label %%foriter.body.%d, label %%foriter.end.%d\n",
                 cmp, id, id);

            emit(cg, "foriter.body.%d:\n", id);
            // arr.lltype is like "i32*" — GEP by index to get element
            int gep = next_reg(cg);
            emit(cg, "  %%%d = getelementptr inbounds %s, %s* %s, i32 %%%d\n",
                 gep, base_type, base_type, arr.name, i_val);
            int load = next_reg(cg);
            emit(cg, "  %%%d = load %s, %s* %%%d\n", load, base_type, base_type, gep);
            emit(cg, "  store %s %%%d, %s* %%%d\n", base_type, load, base_type, val_addr);

            cg_block(cg, node->as.for_iter.body);

            // Increment index
            int ic = next_reg(cg);
            emit(cg, "  %%%d = load i32, i32* %%%d\n", ic, idx_addr);
            int ii = next_reg(cg);
            emit(cg, "  %%%d = add i32 %%%d, 1\n", ii, ic);
            emit(cg, "  store i32 %%%d, i32* %%%d\n", ii, idx_addr);
            emit(cg, "  br label %%foriter.cond.%d\n", id);
            emit(cg, "foriter.end.%d:\n", id);
            cg_scope_pop(cg);
            break;
        }

        // ── unsafe { } ───────────────────────
        case NODE_UNSAFE_BLOCK:
            cg->unsafe_depth++;
            cg_block(cg, node);
            cg->unsafe_depth--;
            break;

        // ── Expression statement ─────────────
        default:
            cg_expr(cg, node);
            break;
    }
}

// ─────────────────────────────────────────────
//  Function codegen
// ─────────────────────────────────────────────

// ─────────────────────────────────────────────
//  Entity codegen
//  Emits: LLVM struct type + method functions
// ─────────────────────────────────────────────
static void cg_entity(Codegen *cg, ASTNode *node) {
    const char *ent_name = node->as.entity_def.name;

    // Register entity and collect fields
    EntityInfo *ent = entity_register(ent_name);
    if (!ent) { fprintf(stderr, "Too many entities\n"); return; }

    // Collect fields (NODE_LET members without init)
    for (int i = 0; i < node->as.entity_def.member_count; i++) {
        ASTNode *m = node->as.entity_def.members[i];
        if (m->type == NODE_LET && m->as.let.init == NULL) {
            char ft[64] = "i32";
            if (m->as.let.type_node) lltype_of_node(m->as.let.type_node, ft, sizeof(ft));
            EntityField *f = &ent->fields[ent->field_count];
            strncpy(f->name,   m->as.let.name, 63);
            strncpy(f->lltype, ft, 63);
            ent->field_count++;
        }
    }

    // Register methods with their return types
    for (int i = 0; i < node->as.entity_def.member_count; i++) {
        ASTNode *m = node->as.entity_def.members[i];
        if (m->type != NODE_FN_DEF) continue;
        if (ent->method_count >= 64) break;
        EntityMethod *em = &ent->methods[ent->method_count++];
        strncpy(em->name, m->as.fn_def.name, 63);
        if (m->as.fn_def.return_type)
            lltype_of_node(m->as.fn_def.return_type, em->ret_lltype, 63);
        else
            strncpy(em->ret_lltype, "void", 63);
    }
    // Emit LLVM struct type declaration
    emit(cg, "%%struct.%s = type { ", ent_name);
    for (int i = 0; i < ent->field_count; i++) {
        if (i) emit(cg, ", ");
        emit(cg, "%s", ent->fields[i].lltype);
    }
    emit(cg, " }\n\n");

    // Emit constructor function: @Entity__new(field0, field1, ...) -> %struct.Entity*
    emit(cg, "define %%struct.%s* @%s__new(", ent_name, ent_name);
    for (int i = 0; i < ent->field_count; i++) {
        if (i) emit(cg, ", ");
        emit(cg, "%s %%init.%s", ent->fields[i].lltype, ent->fields[i].name);
    }
    emit(cg, ") {\nentry:\n");
    cg->reg_counter = 1;
    cg_scope_push(cg);

    // Heap-allocate struct via malloc
    int sz_reg = next_reg(cg);
    emit(cg, "  %%%d = getelementptr inbounds %%struct.%s, %%struct.%s* null, i32 1\n", sz_reg, ent_name, ent_name);
    int szi_reg = next_reg(cg);
    emit(cg, "  %%%d = ptrtoint %%struct.%s* %%%d to i64\n", szi_reg, ent_name, sz_reg);
    int raw_reg = next_reg(cg);
    emit(cg, "  %%%d = call i8* @malloc(i64 %%%d)\n", raw_reg, szi_reg);
    int self_reg = next_reg(cg);
    emit(cg, "  %%%d = bitcast i8* %%%d to %%struct.%s*\n", self_reg, raw_reg, ent_name);

    // Store each init value into the struct
    for (int i = 0; i < ent->field_count; i++) {
        int gep = next_reg(cg);
        emit(cg, "  %%%d = getelementptr inbounds %%struct.%s, %%struct.%s* %%%d, i32 0, i32 %d\n",
             gep, ent_name, ent_name, self_reg, i);
        emit(cg, "  store %s %%init.%s, %s* %%%d\n",
             ent->fields[i].lltype, ent->fields[i].name, ent->fields[i].lltype, gep);
    }

    int ret_reg = next_reg(cg);
    emit(cg, "  %%%d = bitcast %%struct.%s* %%%d to %%struct.%s*\n",
         ret_reg, ent_name, self_reg, ent_name);
    emit(cg, "  ret %%struct.%s* %%%d\n", ent_name, ret_reg);
    cg_scope_pop(cg);
    emit(cg, "}\n\n");

    // Emit each method: @Entity__methodName(%struct.Entity* %self, ...params)
    for (int mi = 0; mi < node->as.entity_def.member_count; mi++) {
        ASTNode *m = node->as.entity_def.members[mi];
        if (m->type != NODE_FN_DEF) continue;

        char ret_type[64] = "void";
        if (m->as.fn_def.return_type)
            lltype_of_node(m->as.fn_def.return_type, ret_type, sizeof(ret_type));

        // Mangled name: Entity__method
        emit(cg, "define %s @%s__%s(%%struct.%s* %%self",
             ret_type, ent_name, m->as.fn_def.name, ent_name);

        for (int pi = 0; pi < m->as.fn_def.param_count; pi++) {
            char pt[64];
            lltype_of_node(m->as.fn_def.params[pi].type, pt, sizeof(pt));
            emit(cg, ", %s %%param.%s", pt, m->as.fn_def.params[pi].name);
        }
        emit(cg, ") {\nentry:\n");
        cg->reg_counter = 1;
        cg_scope_push(cg);

        // Bind self
        int self_addr = next_reg(cg);
        emit(cg, "  %%%d = alloca %%struct.%s*\n", self_addr, ent_name);
        emit(cg, "  store %%struct.%s* %%self, %%struct.%s** %%%d\n",
             ent_name, ent_name, self_addr);
        char self_reg_name[32];
        snprintf(self_reg_name, sizeof(self_reg_name), "%%%d", self_addr);

        // Register "self" in scope as a pointer to the struct
        char self_lltype[96];
        snprintf(self_lltype, sizeof(self_lltype), "%%struct.%s*", ent_name);
        cg_scope_define(cg, "self", self_lltype, self_reg_name);

        // Register each field as a scope entry with GEP-from-self encoding
        char stype[96];
        snprintf(stype, sizeof(stype), "%%struct.%s", ent_name);
        for (int fi = 0; fi < ent->field_count; fi++) {
            char field_alloca[256];
            snprintf(field_alloca, sizeof(field_alloca), "self:%s:%d:%s",
                     ent->fields[fi].name, fi, stype);
            cg_scope_define(cg, ent->fields[fi].name,
                            ent->fields[fi].lltype, field_alloca);
        }

        // Bind params
        for (int pi = 0; pi < m->as.fn_def.param_count; pi++) {
            char pt[64];
            lltype_of_node(m->as.fn_def.params[pi].type, pt, sizeof(pt));
            int addr = next_reg(cg);
            emit(cg, "  %%%d = alloca %s\n", addr, pt);
            emit(cg, "  store %s %%param.%s, %s* %%%d\n",
                 pt, m->as.fn_def.params[pi].name, pt, addr);
            char alloca_name[32];
            snprintf(alloca_name, sizeof(alloca_name), "%%%d", addr);
            cg_scope_define(cg, m->as.fn_def.params[pi].name, pt, alloca_name);
        }

        strncpy(cg->current_entity, ent_name, 63);
        if (m->as.fn_def.is_unsafe) cg->unsafe_depth++;
        cg_block(cg, m->as.fn_def.body);
        if (m->as.fn_def.is_unsafe) cg->unsafe_depth--;

        cg->current_entity[0] = '\0';
        if (!strcmp(ret_type,"void"))
            emit(cg, "  ret void\n");
        else if (!strcmp(ret_type,"i32"))
            emit(cg, "  ret i32 0\n");

        cg_scope_pop(cg);
        emit(cg, "}\n\n");
    }
}

static void cg_fn(Codegen *cg, ASTNode *node) {
    char ret_type[64] = "void";
    if (node->as.fn_def.return_type)
        lltype_of_node(node->as.fn_def.return_type, ret_type, sizeof(ret_type));

    // C runtime requires main() to return i32
    int is_main = !strcmp(node->as.fn_def.name, "main");
    if (is_main) strncpy(ret_type, "i32", sizeof(ret_type)-1);

    // Signature
    fprintf(cg->out, "define %s @%s(", ret_type, node->as.fn_def.name);
    for (int i = 0; i < node->as.fn_def.param_count; i++) {
        if (i) fprintf(cg->out, ", ");
        char pt[64];
        lltype_of_node(node->as.fn_def.params[i].type, pt, sizeof(pt));
        fprintf(cg->out, "%s %%param.%s", pt, node->as.fn_def.params[i].name);
    }
    fprintf(cg->out, ") {\nentry:\n");

    cg_scope_push(cg);

    // Alloca each param and store incoming value
    for (int i = 0; i < node->as.fn_def.param_count; i++) {
        char pt[64];
        lltype_of_node(node->as.fn_def.params[i].type, pt, sizeof(pt));
        int addr = next_reg(cg);
        emit(cg, "  %%%d = alloca %s\n", addr, pt);
        emit(cg, "  store %s %%param.%s, %s* %%%d\n",
             pt, node->as.fn_def.params[i].name, pt, addr);
        char alloca_name[64];
        snprintf(alloca_name, sizeof(alloca_name), "%%%d", addr);
        cg_scope_define(cg, node->as.fn_def.params[i].name, pt, alloca_name);
    }

    if (node->as.fn_def.is_unsafe) cg->unsafe_depth++;
    cg_block(cg, node->as.fn_def.body);
    if (node->as.fn_def.is_unsafe) cg->unsafe_depth--;

    // Auto return void/0 if no explicit return
    if (!strcmp(ret_type, "i32"))
        emit(cg, "  ret i32 0\n");
    else if (!strcmp(ret_type, "void"))
        emit(cg, "  ret void\n");

    cg_scope_pop(cg);
    emit(cg, "}\n\n");
}

// ─────────────────────────────────────────────
//  Public: codegen_run
// ─────────────────────────────────────────────
void codegen_run(Codegen *cg, ASTNode *program) {
    if (!program) return;

    // ── Pass 1: collect all string literals ──
    // (walk full AST — simplified: we handle them inline during emit)

    // ── Module header ─────────────────────────
    emit(cg, "; Taipan — LLVM IR (generated by Venom)\n");
    emit(cg, "target triple = \"x86_64-pc-linux-gnu\"\n\n");

    // ── External declarations ─────────────────
    emit(cg, "declare i32 @puts(i8*)\n");
    emit(cg, "declare i32 @printf(i8*, ...)\n");
    emit(cg, "declare i8* @malloc(i32)\n");
    emit(cg, "declare i8* @__taipan_array_new(i32, i32)\n");
    emit(cg, "declare void @free(i8*)\n");
    emit(cg, "declare i32 @__taipan_array_len(i8*)\n");
    emit(cg, "declare i32 @__taipan_str_len(i8*)\n");
    emit(cg, "declare i8* @__taipan_str_concat(i8*, i8*)\n");
    emit(cg, "declare i32 @__taipan_str_eq(i8*, i8*)\n");
    emit(cg, "declare i8* @__taipan_str_slice(i8*, i32, i32)\n");
    // Math
    emit(cg, "declare float @__taipan_sqrt(float)\n");
    emit(cg, "declare float @__taipan_pow(float, float)\n");
    emit(cg, "declare float @__taipan_abs_f(float)\n");
    emit(cg, "declare float @__taipan_floor(float)\n");
    emit(cg, "declare float @__taipan_ceil(float)\n");
    emit(cg, "declare float @__taipan_sin(float)\n");
    emit(cg, "declare float @__taipan_cos(float)\n");
    emit(cg, "declare float @__taipan_tan(float)\n");
    emit(cg, "declare float @__taipan_log(float)\n");
    emit(cg, "declare float @__taipan_log2(float)\n");
    emit(cg, "declare i32 @__taipan_abs_i(i32)\n");
    emit(cg, "declare i32 @__taipan_min_i(i32, i32)\n");
    emit(cg, "declare i32 @__taipan_max_i(i32, i32)\n");
    emit(cg, "declare float @__taipan_min_f(float, float)\n");
    emit(cg, "declare float @__taipan_max_f(float, float)\n\n");

    // ── Top-level function definitions ────────
    cg_scope_push(cg);
    for (int i = 0; i < program->as.program.count; i++) {
        ASTNode *n = program->as.program.stmts[i];
        if (n->type == NODE_FN_DEF)     cg_fn(cg, n);
        else if (n->type == NODE_ENTITY_DEF) cg_entity(cg, n);
    }
    cg_scope_pop(cg);

    // ── String literal constants (emitted after fns) ──
    emit(cg, "\n; String constants\n");
    // Fixed format strings (not interned, always same size)
    fputs("@.fmtd = private unnamed_addr constant [4 x i8] c\"%d\\0A\\00\"\n", cg->out);
    fputs("@.fmtf = private unnamed_addr constant [4 x i8] c\"%f\\0A\\00\"\n", cg->out);
    fputs("@.fmtd_no_nl = private unnamed_addr constant [3 x i8] c\"%d\\00\"\n", cg->out);
    fputs("@.fmtf_no_nl = private unnamed_addr constant [3 x i8] c\"%f\\00\"\n", cg->out);
    fputs("@.fmts = private unnamed_addr constant [3 x i8] c\"%s\\00\"\n", cg->out);
    // std.math extras
    fputs("declare float @__taipan_clamp_f(float, float, float)\n", cg->out);
    fputs("declare i32 @__taipan_clamp_i(i32, i32, i32)\n", cg->out);
    // std.io input
    fputs("declare i8* @__taipan_read_line()\n", cg->out);
    fputs("declare i32 @__taipan_read_int()\n", cg->out);
    fputs("declare float @__taipan_read_float()\n", cg->out);
    // std.string
    fputs("declare i8* @__taipan_str_upper(i8*)\n", cg->out);
    fputs("declare i8* @__taipan_str_lower(i8*)\n", cg->out);
    fputs("declare i8* @__taipan_str_trim(i8*)\n", cg->out);
    fputs("declare i32 @__taipan_str_contains(i8*, i8*)\n", cg->out);
    fputs("declare i8* @__taipan_str_replace(i8*, i8*, i8*)\n", cg->out);
    fputs("declare i8* @__taipan_str_split(i8*, i8*)\n", cg->out);
    fputs("declare i32 @__taipan_str_to_int(i8*)\n", cg->out);
    fputs("declare float @__taipan_str_to_float(i8*)\n", cg->out);
    fputs("declare i8* @__taipan_int_to_str(i32)\n", cg->out);
    fputs("declare i8* @__taipan_float_to_str(float)\n", cg->out);
    // std.rand
    fputs("declare i32 @__taipan_rand_seed(i32)\n", cg->out);
    fputs("declare i32 @__taipan_rand_int(i32, i32)\n", cg->out);
    fputs("declare float @__taipan_rand_float()\n", cg->out);
    // std.time
    fputs("declare i64 @__taipan_time_now()\n", cg->out);
    fputs("declare i64 @__taipan_time_timestamp()\n", cg->out);
    fputs("declare i32 @__taipan_time_sleep(i32)\n", cg->out);
    // std.env
    fputs("declare i8* @__taipan_getenv(i8*)\n", cg->out);
    // std.mem
    fputs("declare i8* @__taipan_mem_copy(i8*, i8*, i32)\n", cg->out);
    fputs("declare i8* @__taipan_mem_zero(i8*, i32)\n", cg->out);
    // std.process
    fputs("declare i32 @__taipan_exec(i8*)\n", cg->out);
    for (int i = 0; i < cg->str_count; i++) {
        const char *s = cg->str_literals[i];
        int len = 0;
        for (const char *p = s; *p; ) {
            if (p[0] == 92 && p[1] && p[2]) { p += 3; }
            else { p++; }
            len++;
        }
        len++;
        emit(cg, "@.str.%d = private unnamed_addr constant [%d x i8] c\"%s\\00\"\n",
             i, len, s);
    }
}

// ─────────────────────────────────────────────
//  Public: codegen_init / codegen_free
// ─────────────────────────────────────────────
void codegen_init(Codegen *cg, FILE *out) {
    memset(cg, 0, sizeof(Codegen));
    cg->out = out ? out : stdout;
}

void codegen_free(Codegen *cg) {
    while (cg->scope) cg_scope_pop(cg);
    for (int i = 0; i < cg->str_count; i++) free(cg->str_literals[i]);
    free(cg->str_literals);
    free(cg->str_ids);
}
