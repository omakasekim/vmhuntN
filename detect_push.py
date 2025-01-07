push_count = 0
pop_count = 0
max_push_count = 0
max_pop_count = 0
consecutive_pushes = []
consecutive_pops = []
max_consecutive_pushes = []
max_consecutive_pops = []

with open('instrace.txt', 'r') as file:
    for line in file:
        parts = line.strip().split(';')
        if len(parts) < 2:
            continue
            
        instruction = parts[1].strip()
        
        if instruction.startswith('push'):
            push_count += 1
            pop_count = 0
            consecutive_pushes.append(f"{parts[0]}: {instruction}")
            consecutive_pops = []
            if push_count > max_push_count:
                max_push_count = push_count
                max_consecutive_pushes = consecutive_pushes.copy()
        elif instruction.startswith('pop'):
            pop_count += 1
            push_count = 0
            consecutive_pops.append(f"{parts[0]}: {instruction}")
            consecutive_pushes = []
            if pop_count > max_pop_count:
                max_pop_count = pop_count
                max_consecutive_pops = consecutive_pops.copy()
        else:
            push_count = 0
            pop_count = 0
            consecutive_pushes = []
            consecutive_pops = []

print(f"\nMaximum consecutive pushes found: {max_push_count}")
if max_push_count > 0:
    print("Push Sequence:")
    for push in max_consecutive_pushes:
        print(push)

print(f"\nMaximum consecutive pops found: {max_pop_count}")
if max_pop_count > 0:
    print("Pop Sequence:")
    for pop in max_consecutive_pops:
        print(pop)