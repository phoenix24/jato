#ifndef JATO__VM_ANNOTATION_H
#define JATO__VM_ANNOTATION_H

struct cafebabe_annotation;
struct cafebabe_class;

struct vm_element_value_pair {
	const char			*name;
	struct vm_object		*value;
};

struct vm_annotation {
	char				*type;
	unsigned long			nr_elements;
	struct vm_element_value_pair	elements[];
};

struct vm_annotation *vm_annotation_parse(const struct cafebabe_class *klass, struct cafebabe_annotation *annotation);
void vm_annotation_free(struct vm_annotation *vma);

#endif /* JATO__VM_ANNOTATION_H */
