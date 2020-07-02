b1=zeros(size(a,1),1);
b3=zeros(size(a,1),1);
b4=zeros(size(a,1),1);
c1=zeros(size(a,1),1);
c2=zeros(size(a,1),1);
d3=zeros(size(a,1),1);
d4=zeros(size(a,1),1);
e3=zeros(size(a,1),1);
e4=zeros(size(a,1),1);
f3=zeros(size(a,1),1);
f4=zeros(size(a,1),1);

%g=zeros(size(a,1),1);
%h=zeros(size(a,1),1);
%j=zeros(size(a,1),1);
%k=zeros(size(a,1),1);
%l=zeros(size(a,1),1);
%m=zeros(size(a,1),1);
%n=zeros(size(a,1),1);
%p=zeros(size(a,1),1);
%r=zeros(size(a,1),1);

s=1;
for i=1:size(a,1)
    if a(i,1)==3
        b1(s,:)=a(i,2);
        b3(s,:)=a(i,4);
        b4(s,:)=a(i,5);
        s=s+1;
    end
end
b1(s+1:i,:)=[];
b3(s+1:i,:)=[];
b4(s+1:i,:)=[];

s=1;
for i=1:size(a,1)
    if a(i,1)==5
        c1(s,:)=a(i,2);
        c2(s,:)=a(i,3);
    end
end
c1(s+1:i,:)=[];
c2(s+1:i,:)=[];

s=1;
for i=1:size(a,1)
    if a(i,1)==7
        d3(s,:)=a(i,4);
        d4(s,:)=a(i,5);
        s=s+1;

    end
end
d3(s+1:i,:)=[];
d4(s+1:i,:)=[];

s=1;
for i=1:size(a,1)
    if a(i,1)==11
        e3(s,:)=a(i,4);
        e4(s,:)=a(i,5);
        s=s+1;
    end
end
e3(s+1:i,:)=[];
e4(s+1:i,:)=[];

s=1;
for i=1:size(a,1)
    if a(i,1)==15
        f3(s,:)=a(i,4);
        f4(s,:)=a(i,5);
        s=s+1;
    end
end
f3(s+1:i,:)=[];
f4(s+1:i,:)=[];

subplot(3,4,3)
hist(c2,8);
title('sp5 on r2')
%AXIS([0 1 0 1])

subplot(3,4,4)
hist(d2,8);
title('sp7 on r2')
%AXIS([0 1 0 2*10^4])

subplot(3,4,5)
hist(b4,8);
title('sp3 on r4')
%AXIS([0 1 0 6*10^4])

subplot(3,4,6)
hist(e4,8);
title('sp11 on r4')
%AXIS([0 1 0 6*10^4])

subplot(3,4,7)
hist(b3,8);
title('sp3 on r3')
%AXIS([0 1 0 2*10^4])

subplot(3,4,8)
hist(d3,8);
title('sp7 on r3')
%AXIS([0 1 0 2*10^4])

subplot(3,4,9)
hist(d4,8);
title('sp7 on r4')
%AXIS([0 1 0 6*10^4])

subplot(3,4,10)
hist(f4,8);
title('sp15 on r4')
%AXIS([0 1 0 6*10^4])

subplot(3,4,11)
hist(e3,8);
title('sp11 on r3')
%AXIS([0 1 0 6*10^4])

subplot(3,4,12)
hist(f3,8);
title('sp15 on r3')
%AXIS([0 1 0 6*10^4])
