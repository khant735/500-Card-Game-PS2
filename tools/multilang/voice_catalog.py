#!/usr/bin/env python3
"""Shared 501D/501L commentator phrase catalogue."""
RANKS=[("2","two"),("3","three"),("4","four"),("5","five"),("6","six"),("7","seven"),("8","eight"),("9","nine"),("10","ten"),("j","jack"),("q","queen"),("k","king"),("a","ace")]
SUITS=[("spades","spades"),("hearts","hearts"),("diamonds","diamonds"),("clubs","clubs")]
BID_SUITS=[("spades","spades"),("hearts","hearts"),("diamonds","diamonds"),("clubs","clubs"),("no_trumps","no trumps")]
NUM={0:"no",1:"one",2:"two",3:"three",4:"four",5:"five",6:"six",7:"seven",8:"eight",9:"nine",10:"ten",11:"eleven",12:"twelve",13:"thirteen"}

def build_clips():
    clips=[]
    def add(name,text): clips.append((name,text))
    add("cards_dealt", "The cards are down. Have a look at the hand.")
    add("you_pass_cpu_7_spades", "You pass. The other side takes the contract at seven spades.")
    add("you_take_trick", "That trick is yours.")
    add("computer_takes_trick", "That trick goes across the table.")
    add("you_make_contract", "You've made the contract.")
    add("you_fail_contract", "The contract gets away from you this hand.")
    add("computer_makes_contract", "Your opponent has made the contract.")
    add("computer_fails_contract", "The other side has missed the contract.")
    add("you_win_match", "That's the match. You've got it.")
    add("computer_wins_match", "That's the match. It goes to your opponent.")

    player_forms=[
        "You go with the {card}.",
        "You put down the {card}.",
        "The {card} is your play.",
        "You settle on the {card}.",
    ]
    opp_forms=[
        "Your opponent puts down the {card}.",
        "They answer with the {card}.",
        "Out comes the {card} from the other side.",
        "The {card} hits the table from across the way.",
        "Across the table, it's the {card}.",
    ]
    card_index=0
    for actor_name,forms in (("you_play",player_forms),("computer_plays",opp_forms)):
        for rcode,rspoken in RANKS:
            for scode,sspoken in SUITS:
                card=f"{rspoken} of {sspoken}"
                add(f"{actor_name}_{rcode}_{scode}", forms[card_index % len(forms)].format(card=card))
                card_index += 1
        joker_text = "You play the joker." if actor_name=="you_play" else "The joker comes down from the other side."
        add(f"{actor_name}_joker", joker_text)

    for level in range(6,14):
        for scode,sspoken in BID_SUITS:
            add(f"you_bid_{level}_{scode}", f"You call {NUM[level]} {sspoken}.")

    for rcode,rspoken in RANKS:
        for scode,sspoken in SUITS:
            add(f"consider_{rcode}_{scode}", f"You're considering the {rspoken} of {sspoken}.")
    add("consider_joker", "You're considering the joker.")

    for level in range(6,14):
        for scode,sspoken in BID_SUITS:
            add(f"consider_bid_{level}_{scode}", f"You're looking at a bid of {NUM[level]} {sspoken}.")

    add("lead_free", "You're on lead. Any legal card can open this trick.")
    for scode,sspoken in SUITS:
        add(f"follow_{scode}", f"{sspoken.capitalize()} were led. Normally you follow {sspoken} if you hold one; the joker is the exception.")
        add(f"void_{scode}", f"You don't hold {sspoken}, so you're free to play another legal card.")
    add("joker_legal", "The joker is legal here, even when a suit has been led.")
    add("selection_wins", "As the table stands, that card would take the trick.")
    add("selection_loses", "As the table stands, that card would not beat what's already down.")
    add("selection_trump", "That's a trump card, so it can cut a non-trump lead.")
    add("selection_illegal", "That choice isn't legal while you still hold the led suit.")

    for scode,sspoken in BID_SUITS:
        if scode=="no_trumps":
            add("trump_no_trumps", "This is no trumps. The led suit decides the trick unless a joker changes it.")
        else:
            add(f"trump_{scode}", f"{sspoken.capitalize()} are trumps for this contract.")

    for level in range(6,14):
        add(f"contract_target_{level}", f"This contract needs {NUM[level]} tricks.")
    for need in range(1,14):
        word = "one more trick" if need==1 else f"{NUM[need]} more tricks"
        add(f"need_{need}_more", f"You still need {word} to make the contract.")
    add("contract_secured", "You've already reached the contract target. The remaining tricks can add control and chips.")
    add("defend_contract", "You're defending this hand. Every trick you deny the contract holder matters.")

    add("bidding_help_0", "You're in the bidding. A higher call means promising more tricks.")
    add("bidding_help_1", "The suit you name becomes trump. No trumps removes that suit advantage.")
    add("bidding_help_2", "Passing gives the other side the fallback contract at seven spades in this build.")
    add("bidding_help_3", "The contract holder takes the widow, then discards back down before play begins.")
    add("rules_standard", "Standard play uses the full fifty-two card deck with no jokers.")
    add("rules_one_joker", "This ruleset adds one joker to the full deck.")
    add("rules_two_jokers", "This ruleset adds both jokers to the full deck.")
    add("rules_original", "Original reduced play starts at nines for a shorter, tighter hand.")
    add("rules_custom", "Custom rules are active, so the deck, hand, widow, bid limits, score target and chips can differ.")

    hesitate=[
        "You're taking a little time over this one.",
        "Still weighing the options. The table can wait a moment.",
        "You're studying the trick before committing.",
        "There's a bit to consider here.",
        "No rush. You're reading the table.",
        "This choice could shape the next lead.",
        "You're holding the decision for another moment.",
        "Plenty of time to think through the hand.",
    ]
    for i,text in enumerate(hesitate): add(f"hesitation_{i}",text)

    opponent_thinks=[
        "Across the table, your opponent is weighing the reply.",
        "The other side takes a moment to read the trick.",
        "A short pause across the table before the next card.",
        "Your opponent is considering the hand.",
    ]
    for i,text in enumerate(opponent_thinks): add(f"opponent_thinks_{i}",text)

    return clips

CLIPS=build_clips()
assert len(CLIPS)==312, len(CLIPS)
