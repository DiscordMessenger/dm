product dmessenger
    id "Discord Messenger for IRIX (Motif)"
    image sw
        id "Discord Messenger Software"
        version VERSION
        order 9999
        subsys base default
            id "Discord Messenger, a Discord-compatible messenger"
            replaces self
            exp DMESSENGER_BASE
            prereq (
                eoe.sw.base 1289434520 maxint
                x_eoe.sw.eoe 1289434520 maxint
                motif_eoe.sw.eoe 1289434520 maxint
            )
        endsubsys
    endimage
endproduct
