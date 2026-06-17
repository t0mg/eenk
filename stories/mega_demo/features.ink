=== feature_weaves ===
"Well, Poirot? Murder or suicide?"
*   "Murder!"
    "And who did it?"
    * *     "Detective-Inspector Japp!"
    * *     "Captain Hastings!"
    * *     "Myself!"
    - -     "You must be joking!"
    * *     "Mon ami, I am deadly serious."
    * *     "If only..."
*   "Suicide!"
    "Really, Poirot? Are you quite sure?"
    * *     "Quite sure."
    * *     "It is perfectly obvious."
-   Mrs. Christie lowered her manuscript a moment. The rest of the writing group sat, open-mouthed.
-> main_menu

=== feature_loops ===
The guard looks at you with weary, bloodshot eyes.
- (loop)
    *   "Where is the prisoner?"
        "None of your business."
    *   "Can I get you a drink?"
        "I'm on duty."
    *   "What's in the box?"
        "It's just a box."
    *   {loop} [Enough talking]
        "I'll leave you be," you say.
        "Yeah, you do that," the guard replies, leaning back.
        -> done
    -   The guard sighs heavily.
    -> loop
- (done)
-> main_menu

=== feature_variables ===
VAR mood = 0
"How are you feeling today?" the doctor asks.
+   "I feel wonderful!"
    ~ mood = mood + 10
+   "I feel terrible."
    ~ mood = mood - 10
+   "I feel nothing."
    ~ mood = mood
-   The doctor writes something on his clipboard. "I see. Your mood score is {mood}."
{ mood > 0:
    "You are remarkably cheerful for someone in your condition."
}
{ mood < 0:
    "You seem quite depressed. We shall have to keep an eye on you."
}
{ mood == 0:
    "A perfectly neutral affect. Fascinating."
}
-> main_menu

=== feature_multivariant ===
The man at the table deals the cards.
*   [Watch him closely]
-   He deals a {~King|Queen|Jack} of {~Hearts|Spades|Diamonds|Clubs}.
The woman beside him sips her drink. {&She looks at you.|She looks away.|She smiles.|She frowns.}
The man clears his throat. {!He speaks for the first time:|He speaks again:|He says,} "Do you want to play?"
-> main_menu

=== feature_tunnels ===
You stand before the great door of the temple.
*   [Enter the temple] -> entering_temple ->
    You emerge from the temple, forever changed.
    -> main_menu

=== entering_temple ===
The air inside is thick with incense.
You see a golden idol on a pedestal.
*   [Take the idol]
    You grab the idol. The temple begins to collapse!
*   [Leave it]
    You wisely decide to leave the idol alone.
-   You turn and run back to the entrance.
->->
