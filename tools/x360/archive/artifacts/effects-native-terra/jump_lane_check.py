from fractions import Fraction as F
# Exact lane-wiring witnesses for the two sparks lerps and wheel-debris tangent.
left=[F(2),F(-3),F(5),F(7)]; right=[F(6),F(9),F(-7),F(11)]
for a,b in [(F(0),F(1,2)),(F(1,4),F(3,4)),(F(49,100),F(99,100))]:
 span=[y-x for x,y in zip(left,right)]
 pa=[a*d+x for d,x in zip(span,left)]; pb=[d*b+x for d,x in zip(span,left)]
 assert pa==[(1-a)*x+a*y for x,y in zip(left,right)]
 assert pb==[(1-b)*x+b*y for x,y in zip(left,right)]
velocity=[F(4),F(6),F(-2),F(3)]; normal=[F(0),F(1),F(0),F(0)]
dot=sum(x*y for x,y in zip(velocity[:3],normal[:3])); tangent=[x-y*dot for x,y in zip(velocity,normal)]
assert tangent==[4,0,-2,3]
print('PASS 3 spark interpolation pairs and tangent projection, including W lane; exact rational lane wiring only')
