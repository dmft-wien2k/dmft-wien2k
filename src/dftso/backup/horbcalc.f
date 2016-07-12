subroutine horbcalc(h,ri_orb,ne,jspin)
  
  USE abcd
  USE orb
  USE param
  USE lolog
  USE rlolog
  USE struct
  implicit real*8 (a-h,o-z)
  
  complex*16      h(nume2,nume2),hso
  complex*16      czero,dd
  real*8          ri_orb(4,4,0:labc,nato,2,2)   
  INTEGER         :: ne(2),jspin,nban2,n_scr,nmatr
!  complex*16      t(ndif*(labc+1)**2*4,2,nume2)   
  complex*16,allocatable ::  t(:,:,:)   

  allocate(t(ndif*(labc+1)**2*4,2,nume2))   

  czero=(0.0d0,0.0d0)
  nban2=ne(1)+ne(2)+nnrlo*2
  n_scr=ne(1)+ne(2)
  nmatr=nban2*(nban2+1)/2
  
  do ibi = 1, nban2
     if (ibi.le.n_scr) then
        if (ibi.gt.ne(1)) then
           iei=ibi-ne(1)
           isi=2
        else
           iei=ibi
           isi=1
        end if
     else
        if ((ibi-n_scr).gt.nnrlo) then
           iei=ibi-ne(1)-nnrlo
           isi=2
        else
           iei=ibi-ne(2)
           isi=1
        end if
     end if
       
     if (jspin.eq.1) then
        isd=1
     else
        isd=isi
     endif
       
     DO isf=1,2
        if (isf.eq.isi) then
           iso=isi
        else
           iso=3
        end if
        if (jspin.eq.1) then
           isu=1
        else
           isu=isf
        endif
        ii=0
        DO index=1,natorb
           l=ll(index)
           indj=iiat(index)
           ity=iat(index)
           DO MF = -L,L
              ILMF = L*L +L +MF +1
              DO l_mat2=1,mrf(l,ity)
                 ii=ii+1
                 t(ii,isf,ibi)=czero
                 do mi= -l,l
                    ILMI = L*L +L +MI +1
                    dd=czero
                    do l_mat1=1,mrf(l,ity)
                       dd=dd+abcdlm(l_mat1,ilmi,indj,iei,isd)*   &
                            ri_orb(l_mat1,l_mat2,l,ity,isd,isu)
                    enddo
                    t(ii,isf,ibi)=t(ii,isf,ibi)+dd* &
                         vv(index,mf,mi,iso) 
                 enddo
              enddo
           enddo
        enddo
     enddo
  enddo
  do  ibi = 1, nban2
     do ibf = 1, ibi
        hsoc  = czero
        ovrlc = czero
        if (ibi.le.n_scr) then
           if (ibi.gt.ne(1)) then
              iei=ibi-ne(1)
              isi=2
           else
              iei=ibi
              isi=1
           end if
        else
           if ((ibi-n_scr).gt.nnrlo) then
              iei=ibi-ne(1)-nnrlo
              isi=2
           else
              iei=ibi-ne(2)
              isi=1
           end if
        end if
        if (ibf.le.n_scr) then
           if (ibf.gt.ne(1)) then
              ief=ibf-ne(1)
              isf=2
           else
              ief=ibf
              isf=1
           end if
        else
           if ((ibf-n_scr).gt.nnrlo) then
              ief=ibf-ne(1)-nnrlo
              isf=2
           else
              ief=ibf-ne(2)
              isf=1
           end if
        end if
        
        if (jspin.eq.1) then
           isd=1
           isu=1
        else
           isd=isi
           isu=isf
        endif
        
        ii=0
        hso=czero
        DO index=1,natorb
           l=ll(index)
           indj=iiat(index)
           ity=iat(index)
           DO MF = -L,L
              ILMF = L*L +L +MF +1
              DO l_mat2=1,mrf(l,ity)
                 ii=ii+1
                 hso=hso+dconjg(abcdlm(l_mat2,ilmf,indj,ief,isu))* &
                      t(ii,isf,ibi)
              enddo
           enddo
        enddo
        
        h(ibf,ibi)=h(ibf,ibi)+hso
        h(ibi,ibf)=dconjg(h(ibf,ibi))
     enddo
  enddo
  deallocate (t) 
END subroutine horbcalc
