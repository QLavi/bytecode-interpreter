let rock, paper, scissor = 0, 1, 2;
let dfsm = 
[
  [rock,    paper,    "paper"],
  [paper,   rock,     "paper"],
  [paper,   scissor,  "scissor"],
  [scissor, paper,    "scissor"],
  [rock,    scissor,  "rock"],
  [scissor, rock,     "rock"]
];

let input = rock;
let AI = paper;

let possible_outcome = null;
for let x=0; x<6; x+=1 {
  possible_outcome = dfsm[x];
  if possible_outcome[0] == input and possible_outcome[1] == AI {
    print possible_outcome[2];
  }
}
