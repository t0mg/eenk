
LIST Inventory = (Key), (Torch), (Amulet)
VAR lastUsed = ()
-> dark 

= dark
It's dark. You can't see. 

- (top)
    -> useSomething ->
    {
    - used(Torch): 
        The torch light illuminates the rocky tunnel ahead.
        -> tunnel
    - used(Amulet): 
        The amulet is glowing faintly. 
        -> top
    - used(()):
        It's too dark to do anything useful with that.
        -> top
    }
    
*   [ Feel for the walls ] 
    They're hard stone.

*   [ Call out ] 
    "Hello!"
    Your voice echoes away into darkness. Nothing comes back. 

-   -> top 

= tunnel
The tunnel ends in a gate. 
~ temp gate_unlocked = false
- (top)
    -> useSomething ->
    {
    - used(Key): 
        {gate_unlocked:
            You lock the gate once more. 
            ~ gate_unlocked = false
        - else: 
            The key fits the gate. It turns. 
            ~ gate_unlocked = true 
        }
        -> top 
    - used(Torch):
        You turn the torch off again.
        -> dark 
    - used(()):
        That doesn't seem helpful here?
        -> top
    }
    
*   {not gate_unlocked} [ Open the gate ] 
    The gate seems to be locked. 

*   {not gate_unlocked} [ Rattle the gate ] 
    You throw your weight against the gate but it doesn't open. 
    
*   {gate_unlocked} [ Open the gate ] 
    You pull the gate and it swings open.
    -> through_gate
    

-   -> top 

= through_gate
    You've escaped!
    -> DONE
    
    
=== useSomething
    ~ lastUsed = ()
    ~ temp items = Inventory
    + +   {items} [ ITEM MENU ] 
            -> offerItem(items)
    -   ->->
    
= offerItem(items)    
    ~ temp item = pop(items) 
    {item:
        +   (did) [ USE {item} ] 
            ~ lastUsed = item
            ->-> 
    }
    { items: -> offerItem(items) } 
    +   [ BACK ] -> useSomething
        
       
    
 === function used(q)
    { came_from(-> useSomething.did):
        ~ return (q && lastUsed == q) || (not q && lastUsed)
    }
    ~ return false

=== function came_from(-> x) 
    ~ return TURNS_SINCE(x) == 0

=== function pop(ref _list) 
    ~ temp el = LIST_MIN(_list) 
    ~ _list -= el
    ~ return el 