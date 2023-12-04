#include <stdlib.h>
#include <assert.h>
#include <printf.h>
#include <stdbool.h>

// =====================================================================================================================
//  Object
// =====================================================================================================================

/// Types of objects that can be allocated in the GC
typedef enum {
    OBJECT_INT,
    OBJECT_PAIR,
} ObjectType;

/// Index of an Object in the VM's memory
typedef int ObjectIndex;

/// Tagged union of Object types
typedef struct {
    ObjectType type;

    union {
        // OBJECT_INT
        int value;

        // OBJECT_PAIR
        struct {
            ObjectIndex first;
            ObjectIndex second;
        };
    };
} Object;

// =====================================================================================================================
//  VM
// =====================================================================================================================

/// Max number of allocated objects.
/// This is necessary if we use `Array<Ptr<Object>>` to track allocated objects.
/// If we threaded a linked list through the Objects, or used `ArrayList<Ptr<Object>>`, we could grow indefinitely.
/// (`static const int` in array definitions tries to make a Variable-Length Array, so it's a macro)
#define OBJECTS_MAX 1024

/// Number of allocations when we trigger the first gc sweep.
/// After each sweep, the threshold will grow/shrink, up to `OBJECTS_MAX`
static const int INITIAL_GC_THRESHOLD = 128;

/// Max number of variables on the VM stack
#define STACK_MAX 256

/// Entry for the table tracking object allocations
typedef struct {
    bool allocated;
    bool marked;
} AllocationTableEntry;

/// Table tracking object allocations. Seems like a big waste of memory.
typedef AllocationTableEntry AllocationTable[OBJECTS_MAX];

/// The virtual machine.
/// Allocated objects are directly pointed to in both `vm.allocated_objects` and `vm.stack`.
/// That's probably a bad idea...
typedef struct {
    /// Number of currently-allocated objects
    int object_count;

    /// Number of allocations before we do a gc sweep
    int gc_threshold;

    /// `allocation_table[obj_idx]` tracks `allocated_objects[obj_idx]`
    AllocationTable allocation_table;

    /// All allocated objects
    Object *allocated_objects[OBJECTS_MAX];

    /// The current size of the VM's stack
    int stack_size;

    /// Stores variables currently in-scope.
    /// Stored as indexes into `allocation_table` and `allocated_objects`
    ObjectIndex stack[STACK_MAX];
} VM;

/// Allocates, and initializes a new `VM`
VM *vm_init() {
    VM *vm = malloc(sizeof(VM));
    *vm = (VM) {
        .object_count = 0,
        .gc_threshold = INITIAL_GC_THRESHOLD,
        .allocation_table = { {.allocated = false, .marked = false,} },
        .allocated_objects = { 0 },
        .stack_size = 0,
        .stack = { 0 },
    };
    return vm;
}

/// Frees the given VM instance
void vm_free(VM *vm) {
    free(vm);
}

/// Get the `Object` at the given index.
Object *vm_get(VM *vm, ObjectIndex index) {
    return vm->allocated_objects[index];
}

/// Pushes the given object to the stack of the given VM.
/// Represents declaring a variable and putting it on the stack
void vm_push(VM *vm, Object *object) {
    assert(vm->object_count < OBJECTS_MAX && "Too many objects");
    assert(vm->stack_size < STACK_MAX && "Stack overflow");

    // Find an open slot for the object.
    int obj_idx = 0;
    while (vm->allocation_table[obj_idx].allocated) {
        obj_idx += 1;
    }

    // Push to VM objects list
    vm->allocation_table[obj_idx].allocated = true;
    vm->allocated_objects[obj_idx] = object;
    vm->object_count += 1;

    // Push to VM stack
    vm->stack[vm->stack_size] = obj_idx;
    vm->stack_size += 1;
}

/// Pops an object's index off the top of the given VM's stack.
ObjectIndex vm_pop(VM *vm) {
    assert(vm->object_count > 0 && "No objects");
    assert(vm->stack_size > 0 && "Stack underflow");

    // Pop from VM stack
    vm->stack_size -= 1;
    return vm->stack[vm->stack_size];
}

/// Pushes a new `OBJECT_INT` to `vm`
void vm_push_int(VM *vm, int n) {
    Object *obj = malloc(sizeof(Object));
    *obj = (Object) {
            .type = OBJECT_INT,
            .value = n,
    };
    vm_push(vm, obj);
}

/// Pushes a new `OBJECT_PAIR` to `vm`.
/// `pair.first` and `pair.second` are popped from the VM's stack.
void vm_push_pair(VM *vm) {
    Object *obj = malloc(sizeof(Object));
    *obj = (Object) {
        .type = OBJECT_PAIR,
        .first = vm_pop(vm),
        .second = vm_pop(vm)
    };
    vm_push(vm, obj);
}

// =====================================================================================================================
//  Garbage Collector
// =====================================================================================================================

/// Marks the given `Object` and all its children.
void gc_mark(VM *vm, ObjectIndex object_index) {
    AllocationTableEntry *table_entry = &vm->allocation_table[object_index];
    assert(table_entry->allocated && "Object not allocated");

    // If object is marked, return.
    // If object is not marked, mark it and anything it refers to
    if (table_entry->marked) {
        return;
    } else {
        table_entry->marked = true;

        Object *obj = vm->allocated_objects[object_index];
        switch (obj->type) {
            case OBJECT_INT: return;
            case OBJECT_PAIR: {
                gc_mark(vm, obj->first);
                gc_mark(vm, obj->second);
                return;
            }
        }
    }
}

/// Marks all objects in VM memory that are still reachable from a root object on the VM stack.
void gc_mark_all(VM *vm) {
    for (int i = 0; i < vm->stack_size; i++) {
        gc_mark(vm, vm->stack[i]);
    }
}

/// Sweeps through VM memory. Unmarks all marked objects. Frees all unmarked objects.
void gc_sweep(VM *vm) {
    for (int obj_idx = 0; obj_idx < OBJECTS_MAX; obj_idx++) {
        AllocationTableEntry *ate = &vm->allocation_table[obj_idx];
        if (!ate->allocated) break;
        else if (ate->marked) {
            // Unmark marked objects
            ate->marked = false;
        } else {
            // Free unmarked objects
            ate->allocated = false;
            Object *unmarked_obj = vm->allocated_objects[obj_idx];
            free(unmarked_obj);
            vm->object_count -= 1;
        }
    }
}


// =====================================================================================================================
//  CLI
// =====================================================================================================================

// TODO:
// - Trace operations to console
// - Parse, run file
// - Read input in a REPL

int main() {
    // Make a VM, push things
    VM *vm = vm_init();
    vm_push_int(vm, 11);
    vm_push_int(vm, 22);
    vm_push_pair(vm);
    vm_push_int(vm, 33);
    vm_pop(vm); // `33` is popped, leaks in VM memory

    // Mark all in-use objects. Sweep them.
    gc_mark_all(vm);
    gc_sweep(vm);

    // Cleanup
    vm_free(vm);
    return 0;
}
