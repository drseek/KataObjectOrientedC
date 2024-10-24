// kata code, serves no purpose

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_ID 0U
#define MAX_DELEGATE_STORE_CAPACITY 1024U

#define UNUSED(x) (void)(x)

typedef void* (*constructor_t) (void*, va_list*);
typedef void* (*copy_constructor_t) (void*, void*);
typedef void (*destructor_t) (void*);
typedef void* (*assignment_t) (void*, void*);

typedef void (*delegate_function_t) (void*, void*);

typedef struct class
{
    const size_t size;
    const constructor_t constructor;
    const copy_constructor_t copy_constructor;
    const destructor_t destructor;
    const assignment_t assignment;
} class_t, *pclass_t;

void* new (const pclass_t class, ...)
{
    if (class && class->size)
    {
        void* object = malloc (class->size);
        if (object)
        {
            *((pclass_t*) object) = class;
            if (class->constructor)
            {
                va_list args;
                va_start (args, class);
                object = (*class->constructor) (object, &args);
                va_end (args);
            }
        }

        return object;
    }

    return (void*) 0;
}

void* new_copy (void* src)
{
    if (src)
    {
        const pclass_t class = *((pclass_t*) src);
        if (class && class->size)
        {
            void* object = malloc (class->size);
            if (object)
            {
                *((pclass_t*) object) = class;
                if (class->copy_constructor)
                    object = (*class->copy_constructor) (object, src);
            }

            return object;
        }
    }

    return (void*) 0;
}

void delete (void* self)
{
    if (self)
    {
        const pclass_t class = *((pclass_t*) self);
        if (class && class->destructor)
            (*class->destructor) (self);
        free (self);
    }
}

void* assign (void* dst, void* src)
{
    if (dst == src)
        return dst;

    if (dst && src)
    {
        const pclass_t dst_class = *((pclass_t*) dst);
        const pclass_t src_class = *((pclass_t*) src);
        if (dst_class == src_class)
        {
            if (dst_class->assignment)
                return (*dst_class->assignment) (dst, src);
        }
    }

    return (void*) 0;
}

typedef struct delegate_entry
{
    const pclass_t class;
    unsigned id;
    delegate_function_t function;
} delegate_entry_t, *pdelegate_entry_t;

pdelegate_entry_t init_delegate_entry (pdelegate_entry_t self, unsigned id, delegate_function_t function);

void* delegate_entry_constructor (void* self, va_list* args)
{
    pdelegate_entry_t delegate_entry = self;
    unsigned id = va_arg (*args, unsigned);
    delegate_function_t function = va_arg (*args, delegate_function_t);
    return init_delegate_entry (delegate_entry, id, function);
}

void* delegate_entry_copy_constructor (void* dst, void* src)
{
    pdelegate_entry_t dst_delegate_entry = dst;
    pdelegate_entry_t src_delegate_entry = src;

    dst_delegate_entry->id = src_delegate_entry->id;
    dst_delegate_entry->function = src_delegate_entry->function;

    return dst_delegate_entry;
}

void* delegate_entry_assignment (void* dst, void* src)
{
    pdelegate_entry_t dst_delegate_entry = dst;
    pdelegate_entry_t src_delegate_entry = src;

    dst_delegate_entry->id = src_delegate_entry->id;
    dst_delegate_entry->function = src_delegate_entry->function;

    return dst_delegate_entry;
}

const class_t delegate_entry_class_object = {sizeof (delegate_entry_t), &delegate_entry_constructor, &delegate_entry_copy_constructor, (void*) 0, &delegate_entry_assignment};
const pclass_t delegate_entry_class = (const pclass_t) &delegate_entry_class_object;

pdelegate_entry_t init_delegate_entry (pdelegate_entry_t self, unsigned id, delegate_function_t function)
{
    self->id = id;
    self->function = function;

    return self;
}

unsigned delegate_entry_get_id (void* self)
{
    return ((pdelegate_entry_t) self)->id;
}

void delegate_entry_invoke (void* self, void* user, void* username_changed_event_args)
{
    const pdelegate_entry_t delegate_entry = self;
    if (delegate_entry->function)
        (*delegate_entry->function) (user, username_changed_event_args);
}

typedef struct delegate
{
    const pclass_t class;
    unsigned id;
    void** delegate_store;
    size_t delegate_store_capacity;
    size_t delegate_store_size;
} delegate_t, *pdelegate_t;

void* delegate_constructor (void* self, va_list* args)
{
    UNUSED (args);

    pdelegate_t delegate = self;
    delegate->id = 1U;
    delegate->delegate_store_capacity = 8U;
    delegate->delegate_store_size = 0U;
    delegate->delegate_store = calloc (delegate->delegate_store_capacity, sizeof (void*));
    if (delegate->delegate_store)
        memset (delegate->delegate_store, 0, delegate->delegate_store_capacity * sizeof (void*));

    return delegate;
}

void delegate_destructor (void* self)
{
    pdelegate_t delegate = self;
    for (size_t i = 0U; i < delegate->delegate_store_size; ++i)
        delete (delegate->delegate_store[i]);
    free (delegate->delegate_store);
}

const class_t delegate_class_object = {sizeof (delegate_t), &delegate_constructor, (void*) 0, &delegate_destructor, (void*) 0};
const pclass_t delegate_class = (const pclass_t) &delegate_class_object;

unsigned delegate_add (void* self, const delegate_function_t function)
{
    pdelegate_t delegate = self;

    if (!function)
        return INVALID_ID;

    if (UINT_MAX == delegate->id || MAX_DELEGATE_STORE_CAPACITY == delegate->delegate_store_size)
        return INVALID_ID;

    if (delegate->delegate_store_size == delegate->delegate_store_capacity)
    {
        delegate->delegate_store_capacity <<= 1;
        void** new_delegate_store = calloc (delegate->delegate_store_capacity, sizeof (void*));
        if (new_delegate_store)
        {
            memset (new_delegate_store, 0, delegate->delegate_store_capacity * sizeof (void*));

            for (size_t i = 0U; i < delegate->delegate_store_size; ++i)
                new_delegate_store[i] = delegate->delegate_store[i];

            free (delegate->delegate_store);
            delegate->delegate_store = new_delegate_store;
        }
        else
        {
            delegate->delegate_store_capacity >>= 1;
            return INVALID_ID;
        }
    }

    const unsigned new_id = delegate->id++;
    delegate->delegate_store[delegate->delegate_store_size++] = new (delegate_entry_class, new_id, function);
    return new_id;
}

void delegate_remove (void* self, const unsigned id_to_remove)
{
    pdelegate_t delegate = self;

    int found_id = 0;
    size_t i = 0U;

    for (; i < delegate->delegate_store_size; ++i)
    {
        if (id_to_remove == delegate_entry_get_id (delegate->delegate_store[i]))
        {
            found_id = 1;
            delete (delegate->delegate_store[i]);
            delegate->delegate_store[i] = (void*) 0;
            break;
        }
    }

    if (1 == found_id)
    {
        const int last_element_removed = i == delegate->delegate_store_size - 1U ? 1 : 0;

        for (size_t j = i + 1U; j < delegate->delegate_store_size; ++i, ++j)
            delegate->delegate_store[i] = delegate->delegate_store[j];
        
        if (!last_element_removed)
            delegate->delegate_store[delegate->delegate_store_size - 1U] = (void*) 0;
        
        --delegate->delegate_store_size;
    }
}

void delegate_invoke (void* self, void* user, void* username_changed_event_args)
{
    const pdelegate_t delegate = self;

    for (size_t i = 0U; i < delegate->delegate_store_size; ++i)
        delegate_entry_invoke (delegate->delegate_store[i], user, username_changed_event_args);
}

typedef struct username_changed_event_args
{
    const pclass_t class;
    char* old_name;
    char* new_name;
} username_changed_event_args_t, *pusername_changed_event_args_t;

void* init_username_changed_event_args (pusername_changed_event_args_t self, const char* old_name, const char* new_name);

void* username_changed_event_args_constructor (void* self, va_list* args)
{
    pusername_changed_event_args_t username_changed_event_args = self;
    const char* old_name = va_arg (*args, char*);
    const char* new_name = va_arg (*args, char*);
    return init_username_changed_event_args (username_changed_event_args, old_name, new_name);
}

void* username_changed_event_args_copy_constructor (void* dst, void* src)
{
    pusername_changed_event_args_t dst_username_changed_event_args = dst;
    const pusername_changed_event_args_t src_username_changed_event_args = src;

    if (src_username_changed_event_args->old_name)
    {
        const size_t length = strlen (src_username_changed_event_args->old_name);
        if (length)
        {
            dst_username_changed_event_args->old_name = malloc (length + 1U);
            if (dst_username_changed_event_args->old_name)
                strncpy (dst_username_changed_event_args->old_name, src_username_changed_event_args->old_name, length + 1U);
        }
        else
            dst_username_changed_event_args->old_name = (char*) 0;
    }
    else
        dst_username_changed_event_args->old_name = (char*) 0;
    
    if (src_username_changed_event_args->new_name)
    {
        const size_t length = strlen (src_username_changed_event_args->new_name);
        if (length)
        {
            dst_username_changed_event_args->new_name = malloc (length + 1U);
            if (dst_username_changed_event_args->new_name)
                strncpy (dst_username_changed_event_args->new_name, src_username_changed_event_args->new_name, length + 1U);
        }
        else
            dst_username_changed_event_args->new_name = (char*) 0;
    }
    else
        dst_username_changed_event_args->new_name = (char*) 0;
    
    return dst_username_changed_event_args;
}

void username_changed_event_args_destructor (void* self)
{
    pusername_changed_event_args_t username_changed_event_args = self;
    free (username_changed_event_args->old_name);
    free (username_changed_event_args->new_name);
}

void* username_changed_event_args_assignment (void* dst, void* src)
{
    pusername_changed_event_args_t dst_username_changed_event_args = dst;
    const pusername_changed_event_args_t src_username_changed_event_args = src;

    free (dst_username_changed_event_args->old_name);
    
    if (src_username_changed_event_args->old_name)
    {
        const size_t length = strlen (src_username_changed_event_args->old_name);
        if (length)
        {
            dst_username_changed_event_args->old_name = malloc (length + 1U);
            if (dst_username_changed_event_args->old_name)
                strncpy (dst_username_changed_event_args->old_name, src_username_changed_event_args->old_name, length + 1U);
        }
        else
            dst_username_changed_event_args->old_name = (char*) 0;
    }
    else
        dst_username_changed_event_args->old_name = (char*) 0;

    free (dst_username_changed_event_args->new_name);

    if (src_username_changed_event_args->new_name)
    {
        const size_t length = strlen (src_username_changed_event_args->new_name);
        if (length)
        {
            dst_username_changed_event_args->new_name = malloc (length + 1U);
            if (dst_username_changed_event_args->new_name)
                strncpy (dst_username_changed_event_args->new_name, src_username_changed_event_args->new_name, length + 1U);
        }
        else
            dst_username_changed_event_args->new_name = (char*) 0;
    }
    else
        dst_username_changed_event_args->new_name = (char*) 0;

    return dst_username_changed_event_args;
}

const class_t username_changed_event_args_class_object = {sizeof (username_changed_event_args_t), &username_changed_event_args_constructor, &username_changed_event_args_copy_constructor, &username_changed_event_args_destructor, &username_changed_event_args_assignment};
const pclass_t username_changed_event_args_class = (const pclass_t) &username_changed_event_args_class_object;

void* init_username_changed_event_args (pusername_changed_event_args_t self, const char* old_name, const char* new_name)
{
    if (old_name)
    {
        const size_t length = strlen (old_name);
        if (length)
        {
            self->old_name = malloc (length + 1U);
            if (self->old_name)
                strncpy (self->old_name, old_name, length + 1U);
        }
        else
            self->old_name = (char*) 0;
    }
    else
        self->old_name = (char*) 0;

    if (new_name)
    {
        const size_t length = strlen (new_name);
        if (length)
        {
            self->new_name = malloc (length + 1U);
            if (self->new_name)
                strncpy (self->new_name, new_name, length + 1U);
        }
        else
            self->new_name = (char*) 0;
    }
    else
        self->new_name = (char*) 0;
    
    return self;
}

const char* username_changed_event_args_get_old_name (void* self)
{
    return ((const pusername_changed_event_args_t) self)->old_name;
}
		
const char* username_changed_event_args_get_new_name (void* self)
{
    return ((const pusername_changed_event_args_t) self)->new_name;
}

typedef struct user
{
    const pclass_t class;
    char* name;
    void* on_username_changed;
} user_t, *puser_t;

void* init_user (puser_t self, const char* name);

void* user_constructor (void* self, va_list* args)
{
    puser_t user = self;
    const char* name = va_arg (*args, char*);
    return init_user (user, name);
}

void user_destructor (void* self)
{
    puser_t user = self;
    free (user->name);
    delete (user->on_username_changed);
}

const class_t user_class_object = {sizeof (user_t), &user_constructor, (void*) 0, &user_destructor, (void*) 0};
const pclass_t user_class = (const pclass_t) &user_class_object;

void* init_user (puser_t self, const char* name)
{
    if (name)
    {
        const size_t length = strlen (name);
        if (length)
        {
            self->name = malloc (length + 1U);
            if (self->name)
                strncpy (self->name, name, length + 1U);
        }
        else
            self->name = (char*) 0;
    }
    else
        self->name = (char*) 0;
    
    self->on_username_changed = new (delegate_class);

    return self;
}

const char* user_get_name (void* self)
{
    return ((const puser_t) self)->name;
}

void user_set_name (void* self, const char* new_name)
{
    puser_t user = self;
    void* username_changed_event_args = new (username_changed_event_args_class, user->name, new_name);
    
    if (username_changed_event_args)
    {
        free (user->name);
        
        if (new_name)
        {
            const size_t length = strlen (new_name);
            if (length)
            {
                user->name = malloc (length + 1U);
                if (user->name)
                    strncpy (user->name, new_name, length + 1U);
            }
            else
                user->name = (char*) 0;
        }
        else
            user->name = (char*) 0;
        
        delegate_invoke (user->on_username_changed, user, username_changed_event_args);
        delete (username_changed_event_args);
    }
}

unsigned user_add_username_changed_handler (void* self, const delegate_function_t username_changed_handler)
{
    return delegate_add (((puser_t) self)->on_username_changed, username_changed_handler);
}

void user_remove_username_changed_handler (void* self, const unsigned id)
{
    delegate_remove (((puser_t) self)->on_username_changed, id);
}

void name_changed (void* user, void* event_args);

int main ()
{
    void* john = new (user_class, "John");
    user_add_username_changed_handler (john, &name_changed);
    user_set_name (john, "Johnny");
    delete (john);

    return 0;
}

void name_changed (void* user, void* username_changed_event_args)
{
    UNUSED (user);

    printf ("Name changed from %s to %s\n", username_changed_event_args_get_old_name (username_changed_event_args), username_changed_event_args_get_new_name (username_changed_event_args));
}
