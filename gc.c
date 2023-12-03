#include <stdlib.h>
#include <assert.h>
#include <printf.h>

/// Types of objects that can be allocated in the GC
typedef enum {
    OBJECT_INT,
    OBJECT_PAIR,
} ObjectType;

/// Tagged union of Object types
typedef struct Object {
    ObjectType type;

    union {
        // OBJECT_INT
        int value;

        // OBJECT_PAIR
        struct {
            struct Object *first; // Can't use a self-referential typedef `Object *`, must use `struct Object *`
            struct Object *second;
        };
    };
} Object;

/// Make an `Object` of

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

/// The virtual machine.
/// Allocated objects are directly pointed to in both `vm.allocated_objects` and `vm.stack`.
/// That's probably a bad idea...
typedef struct {
    /// Number of currently-allocated objects
    int object_count;

    /// Number of allocations before we do a gc sweep
    int gc_threshold;

    /// All allocated objects
    Object *allocated_objects[OBJECTS_MAX];

    /// The current size of the VM's stack
    int stack_size;

    /// Stores variables currently in-scope
    Object *stack[STACK_MAX];
} VM;

/// Allocates, and initializes a new `VM`
VM *vm_init() {
    VM *vm = malloc(sizeof(VM));
    *vm = (VM) {
        .object_count = 0,
        .gc_threshold = INITIAL_GC_THRESHOLD,
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

/// Pushes the given object to the stack of the given VM.
/// Represents declaring a variable and putting it on the stack
void vm_push(VM *vm, Object *object) {
    assert(vm->object_count < OBJECTS_MAX && "Too many objects");
    assert(vm->stack_size < STACK_MAX && "Stack overflow");

    // Push to VM objects list
    vm->allocated_objects[vm->object_count] = object;
    vm->object_count += 1;

    // Push to VM stack
    vm->stack[vm->stack_size] = object;
    vm->stack_size += 1;
}

/// Pops an `Object` off the top of the given VM's stack.
Object *vm_pop(VM *vm) {
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


int main() {
    VM *vm = vm_init();
    vm_push_int(vm, 22);
    vm_push_int(vm, 44);
    vm_push_pair(vm);
    printf("%d %d", vm->stack[0]->first->value, vm->stack[0]->second->value);
    return 0;
}
